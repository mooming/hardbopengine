// Created by mooming.go@gmail.com

#include "MultiPoolAllocator.h"

#include <algorithm>
#include <bit>
#include <map>
#include "Config/BuildConfig.h"
#include "Engine/Engine.h"
#include "Log/Logger.h"
#include "Memory/MemoryManager.h"
#include "String/StringBuilder.h"
#include "System/SystemStatistics.h"

namespace hbe
{

	MultiPoolAllocator::MultiPoolAllocator(const char* inName, size_t allocationUnit, size_t minBlockSize)

		:
		id(InvalidAllocatorID), parentID(InvalidAllocatorID), name(inName), bankSize(allocationUnit),
		minBlock(minBlockSize)
	{
		using namespace HSTL;

		parentID = hbe::MemoryManager::GetCurrentAllocatorID();

		auto allocFunc = [](void* allocatorPtr, size_t n) -> void*
		{
			auto allocator = static_cast<MultiPoolAllocator*>(allocatorPtr);
			return allocator->Allocate(n);
		};

		auto deallocFunc = [](void* allocatorPtr, void* ptr, size_t n)
		{
			auto allocator = static_cast<MultiPoolAllocator*>(allocatorPtr);
			allocator->Deallocate(ptr, n);
		};

		auto& mmgr = MemoryManager::GetInstance();
		GenerateBanksByCache(mmgr);

		size_t capacity = 0;
		for (auto& bank : banks)
		{
			capacity += bank.GetCapacity();
		}

		id = mmgr.RegisterAllocator(this, name, false, capacity, allocFunc, deallocFunc);
	}

	MultiPoolAllocator::MultiPoolAllocator(const char* inName, TInitializerList initialConfigurations,
										   size_t allocationUnit, size_t minBlockSize)

		:
		id(InvalidAllocatorID), parentID(InvalidAllocatorID), name(inName), bankSize(allocationUnit),
		minBlock(minBlockSize)
	{
		using namespace HSTL;

		parentID = hbe::MemoryManager::GetCurrentAllocatorID();

		auto allocFunc = [](void* allocatorPtr, size_t n) -> void*
		{
			auto allocator = static_cast<MultiPoolAllocator*>(allocatorPtr);
			return allocator->Allocate(n);
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
			capacity += bank.GetCapacity();
		}

		auto& mmgr = MemoryManager::GetInstance();
		id = mmgr.RegisterAllocator(this, name, false, capacity, allocFunc, deallocFunc);
	}

	MultiPoolAllocator::~MultiPoolAllocator()
	{
		auto& mmgr = MemoryManager::GetInstance();

#if PROFILE_ENABLED
		ReportConfiguration();
#endif // PROFILE_ENABLED

#if __MEMORY_VERIFICATION__
		const auto currentTID = std::this_thread::get_id();
		for (auto& bank : banks)
		{
			auto& proxy = mmgr.GetAllocatorProxy(bank.GetID());
			proxy.threadId = currentTID;
		}
#endif // __MEMORY_VERIFICATION__

		banks.clear();

		mmgr.DeregisterAllocator(GetID());
	}

	void* MultiPoolAllocator::Allocate(size_t size)
	{
		if (unlikely(size <= 0))
		{
			return nullptr;
		}

		auto index = GetBankIndex(size);
		if (index >= banks.size())
		{
			// Failed to find a suitable bank for the given size
			// Generate a new bank and allocate
			return NewBankAllocate(size);
		}

		auto& bank = banks[index];
		if (unlikely(bank.GetAvailableBlocks() <= 0))
		{
			// The bank is full. Need to generate a new bank
			return NewBankAllocate(size);
		}

		auto ptr = bank.Allocate(size);

#if PROFILE_ENABLED
		{
			auto& mmgr = MemoryManager::GetInstance();
			mmgr.ReportAllocation(GetID(), ptr, size, bank.GetBlockSize());
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

		auto index = GetBankIndex(ptr);
		if (index >= banks.size())
		{
			auto log = Logger::Get(name);
			log.OutFatalError([ptr, this](auto& ls)
			{
				ls << ptr << " is not allocated by this allocator.";
				for (auto& bank : banks)
				{
					Byte* start = bank.GetBuffer();
					void* end = (start + bank.GetCapacity());
					ls << '\n' << "Bank PTR: " << static_cast<void*>(start) << " - " << end;
				}
			});

			return;
		}

		auto& bank = banks[index];
		Assert(size <= bank.GetBlockSize());

		bank.Deallocate(ptr, size);

#if PROFILE_ENABLED
		{
			auto& mmgr = MemoryManager::GetInstance();
			mmgr.ReportDeallocation(GetID(), ptr, size, bank.GetBlockSize());
		}
#endif // PROFILE_ENABLED
	}

	void MultiPoolAllocator::PrintUsage() const
	{
		using namespace std;

#if PROFILE_ENABLED
		std::map<size_t, size_t> usageMap;
#endif // PROFILE_ENABLED

		auto log = Logger::Get(name, ELogLevel::Info);
		log.Out("= Allocator Usage ========================================");

#if PROFILE_ENABLED
		auto& mmgr = MemoryManager::GetInstance();
		auto stats = mmgr.GetAllocatorStat(GetID());

		log.Out([&stats](auto& ls) { ls << "Usage = " << stats.usage << " / " << stats.maxUsage; });

		log.Out([&stats](auto& ls) { ls << "Capacity = " << stats.capacity; });

		log.Out([&stats](auto& ls)
		{
			ls << "Alloc: " << stats.allocCount << ", Dealloc: " << stats.deallocCount
			   << ", Fallback: " << stats.fallbackCount;
		});
#endif // PROFILE_ENABLED

		for (auto& pool : banks)
		{
#if PROFILE_ENABLED
			log.Out([&pool](auto& ls)
			{
				ls << '[' << pool.GetBlockSize() << "] Memory = " << pool.GetUsage() << " / " << pool.GetCapacity()
				   << ", Max Usage = " << (pool.GetUsedBlocksMax() * pool.GetBlockSize());
			});

			if (pool.GetUsedBlocksMax() > 0)
			{
				auto key = pool.GetBlockSize();
				auto it = usageMap.find(key);

				if (unlikely(it == usageMap.end()))
				{
					usageMap.emplace(key, pool.GetUsedBlocksMax());
				}
				else
				{
					it->second += pool.GetUsedBlocksMax();
				}
			}
#else // PROFILE_ENABLED
			log.Out([&pool](auto& ls)
			{ ls << '[' << pool.GetBlockSize() << "] Memory = " << pool.GetUsage() << " / " << pool.GetCapacity(); });
#endif // PROFILE_ENABLED
		}

#if PROFILE_ENABLED
		{
			AllocatorScope scope(MemoryManager::SystemAllocatorID);

			StringBuilder args;
			args << "Opt.{";

			for (auto& item : usageMap)
			{
				args << " {" << item.first << ", " << item.second << "}, ";
			}
			args << "}";

			log.Out(args.c_str());
		}
#endif // PROFILE_ENABLED

		log.Out("==========================================================");
	}

#if PROFILE_ENABLED
	void MultiPoolAllocator::ReportConfiguration() const
	{
		MemoryManager::TPoolConfigs configs;
		configs.reserve(banks.size());

		auto InsertItem = [&configs](const PoolAllocator& alloc)
		{
			auto key = alloc.GetBlockSize();
			auto value = alloc.GetUsedBlocksMax();

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

		auto& mmgr = MemoryManager::GetInstance();
		auto uniqueName = GetName();
		mmgr.ReportMultiPoolConfigutation(uniqueName.GetID(), std::move(configs));
	}
#endif // PROFILE_ENABLED

	void* MultiPoolAllocator::NewBankAllocate(size_t size)
	{
		auto& engine = Engine::Get();
		auto& statistics = engine.GetStatistics();
		statistics.IncFallbackAllocCount();

		// Create a new bank for the given size
		auto blockSize = CalculateBlockSize(size);
		auto numBlocks = CalculateNumberOfBlocks(bankSize, blockSize);
		GenerateBank(blockSize, numBlocks);

		auto index = GetBankIndex(size);
		if (unlikely(index >= banks.size()))
		{
			FatalAssert(false);
			return nullptr;
		}

		auto& bank = banks[index];
		auto ptr = bank.Allocate(size);

#ifdef PROFILE_ENABLED
		{
			auto& mmgr = MemoryManager::GetInstance();
			mmgr.ReportAllocation(GetID(), ptr, size, bank.GetBlockSize());
		}
#endif // PROFILE_ENABLED

		return ptr;
	}

	bool MultiPoolAllocator::GenerateBanksByCache(MemoryManager& mmgr)
	{
		auto nameID = name.GetID();
		auto& cacheItem = mmgr.LookUpMultiPoolConfig(nameID);
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
	size_t MultiPoolAllocator::GetBankIndex(size_t nBytes) const
	{
		nBytes = std::max(minBlock, nBytes);
		const auto doubleSize = nBytes * 2;

		const auto len = banks.size();
		for (size_t i = 0; i < len; ++i)
		{
			auto& bank = banks[i];
			if (nBytes > bank.GetBlockSize())
			{
				continue;
			}

			if (bank.GetAvailableBlocks() <= 0)
			{
				continue;
			}

			if (bank.GetBlockSize() > doubleSize)
			{
				return len;
			}

			return i;
		}

		return len;
	}

	size_t MultiPoolAllocator::GetBankIndex(void* ptr) const
	{
		size_t index = 0;

		for (auto& bank : banks)
		{
			if (bank.IsMine(ptr))
			{
				return index;
			}

			++index;
		}

		return index;
	}

	size_t MultiPoolAllocator::CalculateBlockSize(size_t requested) const
	{
		size_t blockSize = (requested + minBlock - 1) / minBlock;
		blockSize = std::bit_ceil(blockSize);
		blockSize *= minBlock;
		blockSize = std::max(minBlock, blockSize);

		return blockSize;
	}

	size_t MultiPoolAllocator::CalculateNumberOfBlocks(size_t bankSize, size_t blockSize)
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

	void MultiPoolAllocator::GenerateBank(size_t blockSize, size_t numberOfBlocks)
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

		auto log = Logger::Get(name);
		log.Out(ELogLevel::Verbose, [&str](auto& ls) { ls << "The bank[" << str.c_str() << "] has been generated. "; });

#if PROFILE_ENABLED
		const auto incCapacity = blockSize * numberOfBlocks;
		auto& mmgr = MemoryManager::GetInstance();
		auto& allocProxy = mmgr.GetAllocatorProxy(GetID());
		allocProxy.stats.capacity += incCapacity;
#endif // PROFILE_ENABLED
	}

} // namespace hbe

#ifdef __UNIT_TEST__
#include "AllocatorScope.h"
#include "System/ScopedTime.h"

namespace hbe
{

	void MultiPoolAllocatorTest::Prepare()
	{
		AddTest("Basic Construction", [this](auto& ls)
		{
			MultiPoolAllocator allocator(
					"TestMultiPoolAlloc",
					{{64, 1024}, {128, 1024}, {256, 1024}, {512, 1024}, {1024, 1024}, {2048, 1024}, {4096, 1024}});

#if PROFILE_ENABLED
			auto& mmgr = MemoryManager::GetInstance();
			auto stat = mmgr.GetAllocatorStat(allocator.GetID());
			ls << "Capacity = " << stat.capacity << lf;
#else
			ls << "MultiPoolAllocator has been created" << lf;
#endif // PROFILE_ENABLED

			AllocatorScope scope(allocator);
		});

		AddTest("Allocation 0", [this](auto& ls)
		{
			MultiPoolAllocator allocator(
					"TestMultiPoolAlloc",
					{{64, 1024}, {128, 1024}, {256, 1024}, {512, 1024}, {1024, 1024}, {2048, 1024}, {4096, 1024}});

			auto& mmgr = MemoryManager::GetInstance();

#if PROFILE_ENABLED
			auto stat = mmgr.GetAllocatorStat(allocator.GetID());
			ls << "Capacity = " << stat.capacity << lf;
#else
			ls << "MultiPoolAllocator has been created" << lf;
#endif // PROFILE_ENABLED

			AllocatorScope scope(allocator);
			mmgr.Allocate(0);
		});

		AddTest("Multiple Allocations & Fallback", [this](auto& ls)
		{
			MultiPoolAllocator allocator(
					"TestMultiPoolAlloc",
					{{64, 1024}, {128, 1024}, {256, 1024}, {512, 1024}, {1024, 1024}, {2048, 1024}, {4096, 1024}});

			auto& mmgr = MemoryManager::GetInstance();

#if PROFILE_ENABLED
			{
				AllocStats stat = mmgr.GetAllocatorStat(allocator.GetID());
				ls << "Capacity = " << stat.capacity << lf;
			}
#else
			ls << "MultiPoolAllocator has been created" << lf;
#endif // PROFILE_ENABLED

			AllocatorScope scope(allocator);
			void* pointers[] = {mmgr.Allocate(0),	 mmgr.Allocate(8),	  mmgr.Allocate(16),  mmgr.Allocate(32),
								mmgr.Allocate(97),	 mmgr.Allocate(110),  mmgr.Allocate(140), mmgr.Allocate(270),
								mmgr.Allocate(4032), mmgr.Allocate(5000), mmgr.Allocate(8000)};

			for (auto ptr : pointers)
			{
				mmgr.Deallocate(ptr, 0);
			}

#if PROFILE_ENABLED
			AllocStats stat = mmgr.GetAllocatorStat(allocator.GetID());
			ls << "Capacity = " << stat.capacity << lf;

			if (stat.fallbackCount != 2)
			{
				ls << "Fallback count mismatched. FallbackCount = " << stat.fallbackCount << ", but 2 expected."
				   << lferr;
			}
#endif // PROFILE_ENABLED
		});

		AddTest("Performance", [this](auto& ls)
		{
			Time::TDuration heDuration;
			Time::TDuration stdDuration;

			constexpr size_t repeatCount = 100000;

			MultiPoolAllocator allocator("PerfTestMultiPoolAlloc");

			{
				AllocatorScope allocScope(allocator);
				Time::ScopedTime timer(heDuration);

				int strLen = 1;
				HSTL::HVector<HSTL::HString> v;
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
				Time::ScopedTime timer(stdDuration);

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

			float heSec = Time::ToFloat(heDuration);
			float stdSec = Time::ToFloat(stdDuration);

			ls << "Performance: MultiPoolAllocator = " << heSec << " sec, std malloc = " << stdSec << " sec" << lf;

			if (heSec > stdSec)
			{
				ls << "MultiPoolAllocator is slower than std malloc."
				   << " MultiPoolAllocator  = " << heSec << " sec, std malloc = " << stdSec << " sec" << lfwarn;
			}
		});
	}

} // namespace hbe
#endif //__UNIT_TEST__
