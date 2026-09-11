// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "PoolAllocator.h"


#include "Core/CommonUtil.h"
#include "Core/Debug.h"
#include "MemoryManager.h"
#include "OSAL/OSMemory.h"

namespace hbe
{
#if PROFILE_ENABLED
	PoolAllocator::PoolAllocator(const char* inName, TSize inBlockSize, TSize inNumberOfBlocks,
								 const hbe::source_location location)
#else // PROFILE_ENABLED
	PoolAllocator::PoolAllocator(const char* inName, TSize inBlockSize, TSize inNumberOfBlocks)
#endif // PROFILE_ENABLED
		:
		id(InvalidAllocatorID), parentID(InvalidAllocatorID), name(inName),
		blockSize(OS::getAligned(std::max(inBlockSize, sizeof(TSize)), Config::DefaultAlign)), numberOfBlocks(inNumberOfBlocks),
		numberOfFreeBlocks(inNumberOfBlocks), buffer(nullptr)
#if PROFILE_ENABLED
		,
		maxUsedBlocks(0), srcLocation(location)
#endif // PROFILE_ENABLED
	{
		// A free block has to hold the next-index link written below, so a block smaller than
		// TSize cannot be supported at all - hence a precondition, not something to clamp away.
		Assert(inBlockSize >= sizeof(TSize));

		parentID = hbe::MemoryManager::getCurrentAllocatorID();
		// Both operands are members here: blockSize is the aligned size every later path uses too
		// (AllocateBlock, Deallocate, GetCapacity). Parameters carry the in prefix precisely so no
		// unqualified name here can silently resolve to the unaligned request instead.
		const TSize totalSize = blockSize * numberOfBlocks;
		if (totalSize <= 0)
		{
			return;
		}

		auto& mmgr = MemoryManager::getInstance();
		buffer = static_cast<Byte*>(mmgr.allocate(totalSize));
		availables = &buffer[0];

		for (TSize i = 0; i < numberOfBlocks; ++i)
		{
			auto cursor = &buffer[i * blockSize];
			setAs<TSize>(cursor, i + 1);
		}

		auto allocFunc = [](void* allocatorPtr, size_t n)
		{
			auto allocator = static_cast<PoolAllocator*>(allocatorPtr);
			return allocator->allocate(n);
		};

		auto deallocFunc = [](void* allocatorPtr, void* ptr, size_t n)
		{
			auto allocator = static_cast<PoolAllocator*>(allocatorPtr);
			allocator->Deallocate(ptr, n);
		};

		id = mmgr.registerAllocator(this, inName, false, totalSize, allocFunc, deallocFunc);
		Assert(id != InvalidAllocatorID);
	}

	PoolAllocator::PoolAllocator(PoolAllocator&& rhs) noexcept :
		id(rhs.id), parentID(rhs.parentID), name(rhs.name), blockSize(rhs.blockSize),
		numberOfBlocks(rhs.numberOfBlocks), numberOfFreeBlocks(rhs.numberOfFreeBlocks), buffer(rhs.buffer),
		availables(rhs.availables)
#if PROFILE_ENABLED
		,
		maxUsedBlocks(rhs.maxUsedBlocks), srcLocation(rhs.srcLocation)
#endif // PROFILE_ENABLED
	{
		Assert(id != InvalidAllocatorID);

		rhs.id = InvalidAllocatorID;
		rhs.parentID = InvalidAllocatorID;
		rhs.name = StaticString();
		rhs.blockSize = 0;
		rhs.numberOfBlocks = 0;
		rhs.numberOfFreeBlocks = 0;
		rhs.buffer = nullptr;
		rhs.availables = nullptr;

#if PROFILE_ENABLED
		rhs.maxUsedBlocks = 0;
#endif // PROFILE_ENABLED

		auto& mmgr = MemoryManager::getInstance();

		auto allocFunc = [](void* allocatorPtr, size_t n)
		{
			auto allocator = static_cast<PoolAllocator*>(allocatorPtr);
			return allocator->allocate(n);
		};

		auto deallocFunc = [](void* allocatorPtr, void* ptr, size_t n)
		{
			auto allocator = static_cast<PoolAllocator*>(allocatorPtr);
			allocator->Deallocate(ptr, n);
		};

		auto& allocProxy = mmgr.getAllocatorProxy(getID());

#if PROFILE_ENABLED
		auto& stats = allocProxy.stats;
		stats.capacity = blockSize * numberOfBlocks;
#endif // PROFILE_ENABLED

		allocProxy.allocate = allocFunc;
		allocProxy.deallocate = deallocFunc;
	}

	PoolAllocator::~PoolAllocator()
	{
		if (id == InvalidAllocatorID)
		{
			Assert(buffer == nullptr);
			return;
		}

		const size_t totalSize = blockSize * numberOfBlocks;
		Assert(buffer != nullptr);

		auto& mmgr = MemoryManager::getInstance();
		mmgr.Deallocate(buffer, totalSize);

#if PROFILE_ENABLED
		mmgr.deregisterAllocator(getID(), srcLocation);
#else // PROFILE_ENABLED
		mmgr.deregisterAllocator(getID());
#endif // PROFILE_ENABLED

		buffer = nullptr;
		id = InvalidAllocatorID;
	}

	PoolAllocator& PoolAllocator::operator=(PoolAllocator&& rhs) noexcept
	{
		this->~PoolAllocator();
		new (this) PoolAllocator(std::move(rhs));

		return *this;
	}

	bool PoolAllocator::operator<(const PoolAllocator& rhs) const noexcept
	{
		if (blockSize < rhs.blockSize)
		{
			return true;
		}

		if (blockSize == rhs.blockSize)
		{
			if (getAvailableBlocks() > rhs.getAvailableBlocks())
			{
				return true;
			}
		}

		return false;
	}

	Pointer PoolAllocator::allocate(size_t size)
	{
		if (unlikely(size > blockSize))
		{
			auto& mmgr = MemoryManager::getInstance();
			return mmgr.fallbackAllocate(getID(), parentID, size);
		}

		auto ptr = allocateBlock();
#if PROFILE_ENABLED
		{
			auto& mmgr = MemoryManager::getInstance();
			mmgr.reportAllocation(id, ptr, size, blockSize);
		}
#endif // PROFILE_ENABLED

		return ptr;
	}

	void PoolAllocator::Deallocate(Pointer ptr, size_t size)
	{
		if (unlikely(ptr == nullptr))
		{
			return;
		}

		if (unlikely(!isMine(ptr)))
		{
			auto& mmgr = MemoryManager::getInstance();
			mmgr.Deallocate(parentID, ptr, size);
			return;
		}

#if PROFILE_ENABLED
		{
			Assert(size <= blockSize);
			auto& mmgr = MemoryManager::getInstance();
			mmgr.reportDeallocation(id, ptr, size, blockSize);
		}
#endif // PROFILE_ENABLED

		if (availables)
		{
			const auto index = getIndex(availables);
			if (unlikely(index > numberOfBlocks))
			{
				auto& mmgr = MemoryManager::getInstance();
				mmgr.logError([ptr](auto& logStream) { logStream << ptr << " is not alloacted by this."; });

				return;
			}

			writeNextIndex(ptr, index);
		}
		else
		{
			writeNextIndex(ptr, numberOfBlocks - 1);
		}

		availables = ptr;

		++numberOfFreeBlocks;

		Assert(numberOfFreeBlocks <= numberOfBlocks);
	}

	bool PoolAllocator::isMine(Pointer ptr) const
	{
		auto bytePtr = static_cast<Byte*>(ptr);
		auto offset = static_cast<size_t>(bytePtr - buffer);
		auto totalSize = blockSize * numberOfBlocks;

		return buffer <= bytePtr && offset < totalSize;
	}

	PoolAllocator::TSize PoolAllocator::getIndex(Pointer ptr) const
	{
		auto bytePtr = reinterpret_cast<Byte*>(ptr);
		auto delta = bytePtr - buffer;
		auto index = static_cast<TSize>(delta / blockSize);

		return index;
	}

	PoolAllocator::TSize PoolAllocator::readNextIndex(Pointer ptr) const
	{
		auto index = getAs<TSize>(ptr);
		Assert(index <= numberOfBlocks, "PoolAllocator: out of bounds index = %zu / %zu", index, numberOfBlocks);

		return index;
	}

	void PoolAllocator::writeNextIndex(Pointer ptr, TSize index)
	{
		Assert(index < numberOfBlocks, "PoolAllocator: out of bounds index. The index ", index, " should be less than ",
			   numberOfBlocks);

		setAs<TSize>(ptr, index);
	}

	Pointer PoolAllocator::allocateBlock()
	{
		if (!availables)
		{
			auto& mmgr = MemoryManager::getInstance();
			mmgr.logError([this](auto& ls)
			{
				ls << "No available memory blocks. Usage = " << (numberOfBlocks - numberOfFreeBlocks) << " / "
				   << numberOfBlocks;
			});

			return nullptr;
		}

		void* ptr = availables;
		size_t index = readNextIndex(ptr);

		if (index < numberOfBlocks)
		{
			availables = &buffer[index * blockSize];
		}
		else
		{
			availables = nullptr;
		}

		Assert(numberOfFreeBlocks > 0);
		Assert(numberOfFreeBlocks <= numberOfBlocks);
		--numberOfFreeBlocks;

#if PROFILE_ENABLED
		maxUsedBlocks = std::max(maxUsedBlocks, numberOfBlocks - numberOfFreeBlocks);
#endif // PROFILE_ENABLED

		return ptr;
	}

} // namespace hbe

#ifdef __UNIT_TEST__

namespace hbe
{

	void PoolAllocatorTest::prepare()
	{
		addTest("Construction", [](auto&)
		{
			// Each free block has to hold the pool's next-index link, so a block cannot be
			// narrower than sizeof(size_t) - that is the constructor's precondition, an input
			// error rather than a size worth probing. The range still covers every size above
			// it, including the ones alignment rounds up.
			for (size_t blockSize = sizeof(size_t); blockSize < 100; ++blockSize)
			{
				PoolAllocator pool("TestPoolAllocator", blockSize, 100);
			}
		});

		addTest("Allocation & Deallocation", [this](auto& ls)
		{
			PoolAllocator pool("TestPoolAllocator", 4096, 100);

			for (int i = 0; i < 100; ++i)
			{
				constexpr size_t allocSize = 50;
				auto ptr = pool.allocate(allocSize);
				auto size = pool.getSize(ptr);

				if (size != 4096)
				{
					ls << "The size is incorrect. " << size << ", but 4096 expected." << lferr;
					break;
				}

				pool.Deallocate(ptr, allocSize);
			}
		});

		addTest("Allocation With A Clamped Block Size", [this](auto& ls)
		{
			// The constructor rounds blockSize up to Config::DefaultAlign, so a request that is
			// not already aligned is the case that matters: the pool used to lay its free list out
			// with the requested stride while walking it with the aligned one, which quietly handed
			// the same block to two callers. 32 is here because it needs no rounding and so proves
			// the rounding is not over-reaching; 24 is what LinkedList nodes ask for, and 100 lands
			// mid-way between two multiples.
			struct Case
			{
				size_t requested;
				size_t aligned;
			};

			const Case cases[] = {{32, 32}, {24, 32}, {100, 112}};
			constexpr size_t blockCount = 64;
			constexpr size_t allocSize = 16; // fits every aligned size here, so no fallback path

			for (const Case& testCase : cases)
			{
				PoolAllocator pool("TestPoolAllocatorClamped", testCase.requested, blockCount);

				if (pool.getBlockSize() != testCase.aligned)
				{
					ls << "blockSize " << testCase.requested << " should round up to " << testCase.aligned
					   << ", but " << pool.getBlockSize() << " was used." << lferr;
					return;
				}

				// Twice over. A mis-linked free list survives the first pass and only shows
				// itself once the released blocks are walked again.
				for (size_t round = 0; round < 2; ++round)
				{
					Pointer blocks[blockCount] = {};
					for (size_t i = 0; i < blockCount; ++i)
					{
						blocks[i] = pool.allocate(allocSize);
						if (blocks[i] == nullptr)
						{
							ls << "blockSize " << testCase.requested << ": round " << round
							   << " found no free block at " << i << "." << lferr;
							return;
						}

						for (size_t j = 0; j < i; ++j)
						{
							if (blocks[j] == blocks[i])
							{
								ls << "blockSize " << testCase.requested << ": round " << round << " handed block "
								   << i << " the same address as block " << j << "." << lferr;
								return;
							}
						}

						// Rounding the stride to Config::DefaultAlign only buys 16-byte addresses if
						// the pool buffer itself is aligned, so check the address, not the stride.
						if (!OS::checkAligned(blocks[i]))
						{
							ls << "blockSize " << testCase.requested << ": block " << i
							   << " is not aligned to Config::DefaultAlign." << lferr;
							return;
						}
					}

					for (size_t i = 0; i < blockCount; ++i)
					{
						pool.Deallocate(blocks[i], allocSize);
					}
				}
			}

			ls << "Clamped block sizes keep one stride: no block was handed out twice." << lf;
		});
	}

} // namespace hbe

#endif //__UNIT_TEST__
