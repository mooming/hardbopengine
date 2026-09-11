// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "MultiPoolAllocator.h"


#include <algorithm>
#include <bit>
#include <map>
#include "Config/BuildConfig.h"
#include "Core/SystemStatistics.h"
#include "Engine/Engine.h"
#include "Log/Logger.h"
#include "Memory/MemoryManager.h"
#include "String/StringBuilder.h"

namespace hbe
{

	MultiPoolAllocator::MultiPoolAllocator(const char* inName, size_t allocationUnit, size_t minBlockSize)

		:
		id(InvalidAllocatorID), parentID(InvalidAllocatorID), name(inName), bankSize(allocationUnit),
		minBlock(minBlockSize)
	{
		using namespace hbe;

		parentID = hbe::MemoryManager::getCurrentAllocatorID();

		auto allocFunc = [](void* allocatorPtr, size_t n) -> void*
		{
			auto allocator = static_cast<MultiPoolAllocator*>(allocatorPtr);
			return allocator->allocate(n);
		};

		auto deallocFunc = [](void* allocatorPtr, void* ptr, size_t n)
		{
			auto allocator = static_cast<MultiPoolAllocator*>(allocatorPtr);
			allocator->Deallocate(ptr, n);
		};

		auto& mmgr = MemoryManager::getInstance();
		generateBanksByCache(mmgr);

		size_t capacity = 0;
		for (auto& bank : banks)
		{
			capacity += bank.getCapacity();
		}

		id = mmgr.registerAllocator(this, name, false, capacity, allocFunc, deallocFunc);
	}

	MultiPoolAllocator::MultiPoolAllocator(const char* inName, TInitializerList initialConfigurations,
										   size_t allocationUnit, size_t minBlockSize)

		:
		id(InvalidAllocatorID), parentID(InvalidAllocatorID), name(inName), bankSize(allocationUnit),
		minBlock(minBlockSize)
	{
		using namespace hbe;

		parentID = hbe::MemoryManager::getCurrentAllocatorID();

		auto allocFunc = [](void* allocatorPtr, size_t n) -> void*
		{
			auto allocator = static_cast<MultiPoolAllocator*>(allocatorPtr);
			return allocator->allocate(n);
		};

		auto deallocFunc = [](void* allocatorPtr, void* ptr, size_t n)
		{
			auto allocator = static_cast<MultiPoolAllocator*>(allocatorPtr);
			allocator->Deallocate(ptr, n);
		};

		constexpr size_t InlineBufferSize = 128;
		Assert(initialConfigurations.size() <= InlineBufferSize);

		HInlineVector<PoolConfig, InlineBufferSize> vlist;
		vlist.reserve(InlineBufferSize);

		vlist.insert(vlist.end(), initialConfigurations);
		std::sort(vlist.begin(), vlist.end());

		banks.reserve(vlist.size());

		for (auto& config : vlist)
		{
			InlineStringBuilder<1024> str;
			str << name << '_' << config.blockSize << '_' << config.numberOfBlocks;

			banks.emplace_back(str.c_str(), config.blockSize, config.numberOfBlocks);
		}

		size_t capacity = 0;
		for (auto& bank : banks)
		{
			capacity += bank.getCapacity();
		}

		auto& mmgr = MemoryManager::getInstance();
		id = mmgr.registerAllocator(this, name, false, capacity, allocFunc, deallocFunc);
	}

	MultiPoolAllocator::~MultiPoolAllocator()
	{
		auto& mmgr = MemoryManager::getInstance();

#if PROFILE_ENABLED
		reportConfiguration();
#endif // PROFILE_ENABLED

#if MEMORY_VERIFICATION_ENABLED
		const auto currentTID = std::this_thread::get_id();
		for (auto& bank : banks)
		{
			auto& proxy = mmgr.getAllocatorProxy(bank.getID());
			proxy.threadId = currentTID;
		}
#endif // MEMORY_VERIFICATION_ENABLED

		banks.clear();

		mmgr.deregisterAllocator(getID());
	}

	void* MultiPoolAllocator::allocate(size_t size)
	{
		if (unlikely(size <= 0))
		{
			return nullptr;
		}

		auto index = getBankIndex(size);
		if (index >= banks.size())
		{
			// Failed to find a suitable bank for the given size
			// Generate a new bank and allocate
			return newBankAllocate(size);
		}

		auto& bank = banks[index];
		if (unlikely(bank.getAvailableBlocks() <= 0))
		{
			// The bank is full. Need to generate a new bank
			return newBankAllocate(size);
		}

		auto ptr = bank.allocate(size);

#if PROFILE_ENABLED
		{
			auto& mmgr = MemoryManager::getInstance();
			mmgr.reportAllocation(getID(), ptr, size, bank.getBlockSize());
		}
#endif // PROFILE_ENABLED

		return ptr;
	}

	void MultiPoolAllocator::Deallocate(void* ptr, size_t size)
	{
		if (unlikely(ptr == nullptr))
		{
			Assert(size == 0);
			return;
		}

		auto index = getBankIndex(ptr);
		if (index >= banks.size())
		{
			auto log = Logger::get(name);
			log.outFatalError([ptr, this](auto& ls)
			{
				ls << ptr << " is not allocated by this allocator.";
				for (auto& bank : banks)
				{
					Byte* start = bank.getBuffer();
					void* end = (start + bank.getCapacity());
					ls << '\n' << "Bank PTR: " << static_cast<void*>(start) << " - " << end;
				}
			});

			return;
		}

		auto& bank = banks[index];
		Assert(size <= bank.getBlockSize());

		bank.Deallocate(ptr, size);

#if PROFILE_ENABLED
		{
			auto& mmgr = MemoryManager::getInstance();
			mmgr.reportDeallocation(getID(), ptr, size, bank.getBlockSize());
		}
#endif // PROFILE_ENABLED
	}

	void MultiPoolAllocator::printUsage() const
	{
#if PROFILE_ENABLED
		using namespace std;

		std::map<size_t, size_t> usageMap;

		auto log = Logger::get(name, ELogLevel::Info);
		log.out("= Allocator Usage ========================================");

		auto& mmgr = MemoryManager::getInstance();
		auto stats = mmgr.getAllocatorStat(getID());

		log.out([&stats](auto& ls) { ls << "Usage = " << stats.usage << " / " << stats.maxUsage; });

		log.out([&stats](auto& ls) { ls << "Capacity = " << stats.capacity; });

		log.out([&stats](auto& ls)
		{
			ls << "Alloc: " << stats.allocCount << ", Dealloc: " << stats.deallocCount
			   << ", Fallback: " << stats.fallbackCount;
		});

		for (auto& pool : banks)
		{
			log.out([&pool](auto& ls)
			{
				ls << '[' << pool.getBlockSize() << "] Memory = " << pool.getUsage() << " / " << pool.getCapacity()
				   << ", Max Usage = " << (pool.getUsedBlocksMax() * pool.getBlockSize());
			});

			if (pool.getUsedBlocksMax() > 0)
			{
				auto key = pool.getBlockSize();
				auto it = usageMap.find(key);

				if (unlikely(it == usageMap.end()))
				{
					usageMap.emplace(key, pool.getUsedBlocksMax());
				}
				else
				{
					it->second += pool.getUsedBlocksMax();
				}
			}
		}

		{
			AllocatorScope scope(MemoryManager::SystemAllocatorID);

			StringBuilder args;
			args << "Opt.{";

			for (auto& item : usageMap)
			{
				args << " {" << item.first << ", " << item.second << "}, ";
			}
			args << "}";

			log.out(args.c_str());
		}

		log.out("==========================================================");
#endif // PROFILE_ENABLED
	}

#if PROFILE_ENABLED
	void MultiPoolAllocator::reportConfiguration() const
	{
		MemoryManager::TPoolConfigs configs;
		configs.reserve(banks.size());

		auto InsertItem = [&configs](const PoolAllocator& alloc)
		{
			auto key = alloc.getBlockSize();
			auto value = alloc.getUsedBlocksMax();

			auto pred = [key](const PoolConfig& item) { return item.blockSize == key; };

			auto found = std::find_if(configs.begin(), configs.end(), pred);
			if (found == configs.end())
			{
				configs.emplace_back(key, value);
			}

			found->numberOfBlocks += value;
		};

		for (auto& bank : banks)
		{
			InsertItem(bank);
		}

		auto& mmgr = MemoryManager::getInstance();
		auto uniqueName = getName();
		mmgr.reportMultiPoolConfigutation(uniqueName.getID(), std::move(configs));
	}
#endif // PROFILE_ENABLED

	void* MultiPoolAllocator::newBankAllocate(size_t size)
	{
		auto& engine = Engine::get();
		auto& statistics = engine.getStatistics();
		statistics.incFallbackAllocCount();

		// Create a new bank for the given size
		auto blockSize = calculateBlockSize(size);
		auto numBlocks = calculateNumberOfBlocks(bankSize, blockSize);
		generateBank(blockSize, numBlocks);

		auto index = getBankIndex(size);
		if (unlikely(index >= banks.size()))
		{
			fatalAssert(false);
			return nullptr;
		}

		auto& bank = banks[index];
		auto ptr = bank.allocate(size);

#ifdef PROFILE_ENABLED
		{
			auto& mmgr = MemoryManager::getInstance();
			mmgr.reportAllocation(getID(), ptr, size, bank.getBlockSize());
		}
#endif // PROFILE_ENABLED

		return ptr;
	}

	bool MultiPoolAllocator::generateBanksByCache(MemoryManager& mmgr)
	{
		auto nameID = name.getID();
		auto& cacheItem = mmgr.lookUpMultiPoolConfig(nameID);
		if (cacheItem.uniqueName != nameID)
		{
			return false;
		}

		auto& configs = cacheItem.configs;
		for (auto& config : configs)
		{
			InlineStringBuilder<1024> str;
			str << name << '_' << config.blockSize << '_' << config.numberOfBlocks;

			banks.emplace_back(str.c_str(), config.blockSize, config.numberOfBlocks);
		}

		return true;
	}
	size_t MultiPoolAllocator::getBankIndex(size_t nBytes) const
	{
		nBytes = std::max(minBlock, nBytes);
		const auto doubleSize = nBytes * 2;

		const auto len = banks.size();
		for (size_t i = 0; i < len; ++i)
		{
			auto& bank = banks[i];
			if (nBytes > bank.getBlockSize())
			{
				continue;
			}

			if (bank.getAvailableBlocks() <= 0)
			{
				continue;
			}

			if (bank.getBlockSize() > doubleSize)
			{
				return len;
			}

			return i;
		}

		return len;
	}

	size_t MultiPoolAllocator::getBankIndex(void* ptr) const
	{
		size_t index = 0;

		for (auto& bank : banks)
		{
			if (bank.isMine(ptr))
			{
				return index;
			}

			++index;
		}

		return index;
	}

	size_t MultiPoolAllocator::calculateBlockSize(size_t requested) const
	{
		size_t blockSize = (requested + minBlock - 1) / minBlock;
		blockSize = std::bit_ceil(blockSize);
		blockSize *= minBlock;
		blockSize = std::max(minBlock, blockSize);

		return blockSize;
	}

	size_t MultiPoolAllocator::calculateNumberOfBlocks(size_t bankSize, size_t blockSize)
	{
		size_t numberOfBlocks = (bankSize + blockSize - 1) / blockSize;

		constexpr size_t megaBytes = 1024 * 1024;
		if (blockSize < megaBytes)
		{
			numberOfBlocks = std::max(MinNumberOfBlocks, numberOfBlocks);
		}
		else
		{
			numberOfBlocks = std::max((size_t) 1, numberOfBlocks);
		}

		return numberOfBlocks;
	}

	void MultiPoolAllocator::generateBank(size_t blockSize, size_t numberOfBlocks)
	{
		AllocatorScope scope(parentID);

		Assert(blockSize > 0);
		Assert(numberOfBlocks > 0);

		InlineStringBuilder<1024> str;
		str << name << '_' << blockSize << '_' << numberOfBlocks;

		{
			AllocatorScope allocScope(parentID);
			banks.emplace_back(str.c_str(), blockSize, numberOfBlocks);
			std::sort(banks.begin(), banks.end());
		}

		auto log = Logger::get(name);
		log.out(ELogLevel::Verbose, [&str](auto& ls) { ls << "The bank[" << str.c_str() << "] has been generated. "; });

#if PROFILE_ENABLED
		const auto incCapacity = blockSize * numberOfBlocks;
		auto& mmgr = MemoryManager::getInstance();
		auto& allocProxy = mmgr.getAllocatorProxy(getID());
		allocProxy.stats.capacity += incCapacity;
#endif // PROFILE_ENABLED
	}

} // namespace hbe

#ifdef __UNIT_TEST__
#include "AllocatorScope.h"
#include "Core/ScopedTime.h"

namespace hbe
{

	void MultiPoolAllocatorTest::prepare()
	{
		addTest("Basic Construction", [this](auto& ls)
		{
			MultiPoolAllocator allocator(
					"TestMultiPoolAlloc",
					{{64, 1024}, {128, 1024}, {256, 1024}, {512, 1024}, {1024, 1024}, {2048, 1024}, {4096, 1024}});

#if PROFILE_ENABLED
			auto& mmgr = MemoryManager::getInstance();
			auto stat = mmgr.getAllocatorStat(allocator.getID());
			ls << "Capacity = " << stat.capacity << lf;
#else
			ls << "MultiPoolAllocator has been created" << lf;
#endif // PROFILE_ENABLED

			AllocatorScope scope(allocator);
		});

		addTest("Allocation 0", [this](auto& ls)
		{
			MultiPoolAllocator allocator(
					"TestMultiPoolAlloc",
					{{64, 1024}, {128, 1024}, {256, 1024}, {512, 1024}, {1024, 1024}, {2048, 1024}, {4096, 1024}});

			auto& mmgr = MemoryManager::getInstance();

#if PROFILE_ENABLED
			auto stat = mmgr.getAllocatorStat(allocator.getID());
			ls << "Capacity = " << stat.capacity << lf;
#else
			ls << "MultiPoolAllocator has been created" << lf;
#endif // PROFILE_ENABLED

			AllocatorScope scope(allocator);
			mmgr.allocate(0);
		});

		addTest("Multiple Allocations & Fallback", [this](auto& ls)
		{
			MultiPoolAllocator allocator(
					"TestMultiPoolAlloc",
					{{64, 1024}, {128, 1024}, {256, 1024}, {512, 1024}, {1024, 1024}, {2048, 1024}, {4096, 1024}});

			auto& mmgr = MemoryManager::getInstance();

#if PROFILE_ENABLED
			{
				AllocStats stat = mmgr.getAllocatorStat(allocator.getID());
				ls << "Capacity = " << stat.capacity << lf;
			}
#else
			ls << "MultiPoolAllocator has been created" << lf;
#endif // PROFILE_ENABLED

			AllocatorScope scope(allocator);
			void* pointers[] = {mmgr.allocate(0),	 mmgr.allocate(8),	  mmgr.allocate(16),  mmgr.allocate(32),
								mmgr.allocate(97),	 mmgr.allocate(110),  mmgr.allocate(140), mmgr.allocate(270),
								mmgr.allocate(4032), mmgr.allocate(5000), mmgr.allocate(8000)};

			for (auto ptr : pointers)
			{
				mmgr.Deallocate(ptr, 0);
			}

#if PROFILE_ENABLED
			AllocStats stat = mmgr.getAllocatorStat(allocator.getID());
			ls << "Capacity = " << stat.capacity << lf;

			if (stat.fallbackCount != 2)
			{
				ls << "Fallback count mismatched. FallbackCount = " << stat.fallbackCount << ", but 2 expected."
				   << lferr;
			}
#endif // PROFILE_ENABLED
		});

		addTest("Performance", [this](auto& ls)
		{
			time::TDuration heDuration;
			time::TDuration stdDuration;

			constexpr size_t repeatCount = 100000;

			MultiPoolAllocator allocator("PerfTestMultiPoolAlloc");

			{
				AllocatorScope allocScope(allocator);
				time::ScopedTime timer(heDuration);

				int strLen = 1;
				hbe::HVector<hbe::HString> v;
				for (size_t i = 0; i < repeatCount; ++i)
				{
					auto& str = v.emplace_back();
					for (int j = 0; j < strLen; ++j)
					{
						char ch = 'a' + j;

						if (ch == '\0')
						{
							ch = 'a';
							strLen = 1;
						}

						str.push_back(ch);
					}

					++strLen;
				}
			}

			{
				time::ScopedTime timer(stdDuration);

				int strLen = 1;
				std::vector<std::string> v;
				for (size_t i = 0; i < repeatCount; ++i)
				{
					auto& str = v.emplace_back();
					for (int j = 0; j < strLen; ++j)
					{
						char ch = 'a' + j;
						if (ch == '\0')
						{
							ch = 'a';
							strLen = 1;
						}

						str.push_back(ch);
					}

					++strLen;
				}
			}

			constexpr float kPerfToleranceFactor = 2.0f;
			float heSec = time::toFloat(heDuration);
			float stdSec = time::toFloat(stdDuration);

			ls << "Performance: MultiPoolAllocator = " << heSec << " sec, std malloc = " << stdSec << " sec" << lf;

			if (heSec > stdSec * kPerfToleranceFactor)
			{
				ls << "MultiPoolAllocator is slower than std malloc."
				   << " MultiPoolAllocator  = " << heSec << " sec, std malloc = " << stdSec << " sec" << lfwarn;
			}
		});
	}

} // namespace hbe
#endif //__UNIT_TEST__
