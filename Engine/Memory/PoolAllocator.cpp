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
							 const SourceLocation location)
#else // PROFILE_ENABLED
PoolAllocator::PoolAllocator(const char* inName, TSize inBlockSize, TSize inNumberOfBlocks)
#endif // PROFILE_ENABLED
	: id(InvalidAllocatorID)
	, parentID(InvalidAllocatorID)
	, name(inName)
	, blockSize(OS::GetAligned(std::max(inBlockSize, sizeof(TSize)), Config::DefaultAlign))
	, numberOfBlocks(inNumberOfBlocks)
	, numberOfFreeBlocks(inNumberOfBlocks)
	, buffer(nullptr)
#if PROFILE_ENABLED
	, maxUsedBlocks(0)
	, srcLocation(location)
#endif // PROFILE_ENABLED
{
	Assert(inBlockSize >= sizeof(TSize));

	parentID = hbe::MemoryManager::GetCurrentAllocatorID();
	const TSize totalSize = blockSize * numberOfBlocks;
	if (totalSize <= 0)
	{
		return;
	}

	auto& mmgr = MemoryManager::GetInstance();
	buffer = static_cast<Byte*>(mmgr.Allocate(totalSize));
	availables = &buffer[0];

	for (TSize i = 0; i < numberOfBlocks; ++i)
	{
		auto cursor = &buffer[i * blockSize];
		SetAs<TSize>(cursor, i + 1);
	}

	auto allocFunc = [](void* allocatorPtr, size_t n)
	{
		auto allocator = static_cast<PoolAllocator*>(allocatorPtr);

		return allocator->Allocate(n);
	};

	auto deallocFunc = [](void* allocatorPtr, void* ptr, size_t n)
	{
		auto allocator = static_cast<PoolAllocator*>(allocatorPtr);
		allocator->Deallocate(ptr, n);
	};

	id = mmgr.RegisterAllocator(this, inName, false, totalSize, allocFunc, deallocFunc);
	Assert(id != InvalidAllocatorID);
}

PoolAllocator::PoolAllocator(PoolAllocator&& rhs) noexcept
	: id(rhs.id)
	, parentID(rhs.parentID)
	, name(rhs.name)
	, blockSize(rhs.blockSize)
	, numberOfBlocks(rhs.numberOfBlocks)
	, numberOfFreeBlocks(rhs.numberOfFreeBlocks)
	, buffer(rhs.buffer)
	, availables(rhs.availables)
#if PROFILE_ENABLED
	, maxUsedBlocks(rhs.maxUsedBlocks)
	, srcLocation(rhs.srcLocation)
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

	auto& mmgr = MemoryManager::GetInstance();

	auto allocFunc = [](void* allocatorPtr, size_t n)
	{
		auto allocator = static_cast<PoolAllocator*>(allocatorPtr);

		return allocator->Allocate(n);
	};

	auto deallocFunc = [](void* allocatorPtr, void* ptr, size_t n)
	{
		auto allocator = static_cast<PoolAllocator*>(allocatorPtr);
		allocator->Deallocate(ptr, n);
	};

	auto& allocProxy = mmgr.GetAllocatorProxy(GetID());

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

	auto& mmgr = MemoryManager::GetInstance();
	mmgr.Deallocate(parentID, buffer, totalSize);

#if PROFILE_ENABLED
	mmgr.DeregisterAllocator(GetID(), srcLocation);
#else // PROFILE_ENABLED
	mmgr.DeregisterAllocator(GetID());
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
		if (GetAvailableBlocks() > rhs.GetAvailableBlocks())
		{
			return true;
		}
	}

	return false;
}

Pointer PoolAllocator::Allocate(size_t size)
{
	if (unlikely(size > blockSize))
	{
		auto& mmgr = MemoryManager::GetInstance();

		return mmgr.FallbackAllocate(GetID(), parentID, size);
	}

	auto ptr = AllocateBlock();
#if PROFILE_ENABLED
	{
		auto& mmgr = MemoryManager::GetInstance();
		mmgr.ReportAllocation(id, ptr, size, blockSize);
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

	if (unlikely(!IsMine(ptr)))
	{
		auto& mmgr = MemoryManager::GetInstance();
		mmgr.Deallocate(parentID, ptr, size);

		return;
	}

#if PROFILE_ENABLED
	{
		Assert(size <= blockSize);
		auto& mmgr = MemoryManager::GetInstance();
		mmgr.ReportDeallocation(id, ptr, size, blockSize);
	}
#endif // PROFILE_ENABLED

	if (availables)
	{
		const auto index = GetIndex(availables);
		if (unlikely(index > numberOfBlocks))
		{
			auto& mmgr = MemoryManager::GetInstance();
			mmgr.LogError([ptr](auto& logStream) { logStream << ptr << " is not alloacted by this."; });

			return;
		}

		WriteNextIndex(ptr, index);
	}
	else
	{
		WriteNextIndex(ptr, numberOfBlocks);
	}

	availables = ptr;

	++numberOfFreeBlocks;

	Assert(numberOfFreeBlocks <= numberOfBlocks);
}

bool PoolAllocator::IsMine(Pointer ptr) const
{
	auto bytePtr = static_cast<Byte*>(ptr);
	auto offset = static_cast<size_t>(bytePtr - buffer);
	auto totalSize = blockSize * numberOfBlocks;

	return buffer <= bytePtr && offset < totalSize;
}

PoolAllocator::TSize PoolAllocator::GetIndex(Pointer ptr) const
{
	auto bytePtr = reinterpret_cast<Byte*>(ptr);
	auto delta = bytePtr - buffer;
	auto index = static_cast<TSize>(delta / blockSize);

	return index;
}

PoolAllocator::TSize PoolAllocator::ReadNextIndex(Pointer ptr) const
{
	auto index = GetAs<TSize>(ptr);
	Assert(index <= numberOfBlocks, "PoolAllocator: free-list link ", index, " is out of bounds for ", numberOfBlocks,
			   " blocks.");

	return index;
}

void PoolAllocator::WriteNextIndex(Pointer ptr, TSize index)
{
	Assert(index <= numberOfBlocks, "PoolAllocator: out of bounds index. The index ", index, " should be at most ",
		   numberOfBlocks);

	SetAs<TSize>(ptr, index);
}

Pointer PoolAllocator::AllocateBlock()
{
	if (!availables)
	{
		auto& mmgr = MemoryManager::GetInstance();
		mmgr.LogError([this](auto& ls) {
			ls << "No available memory blocks. Usage = " << (numberOfBlocks - numberOfFreeBlocks) << " / "
			   << numberOfBlocks;
		});

		return nullptr;
	}

	void* ptr = availables;
	size_t index = ReadNextIndex(ptr);

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

#ifdef TEST_ENABLED

namespace hbe
{
void PoolAllocatorTest::Prepare()
{
	AddTest("Construction", [](auto&)
	{
		for (size_t blockSize = sizeof(size_t); blockSize < 100; ++blockSize)
		{
			PoolAllocator pool("TestPoolAllocator", blockSize, 100);
		}
	});

	AddTest("Allocation & Deallocation", [this](auto& ls)
	{
		PoolAllocator pool("TestPoolAllocator", 4096, 100);

		for (int i = 0; i < 100; ++i)
		{
			constexpr size_t allocSize = 50;
			auto ptr = pool.Allocate(allocSize);
			auto size = pool.GetSize(ptr);

			if (size != 4096)
			{
				ls << "The size is incorrect. " << size << ", but 4096 expected." << lferr;
				break;
			}

			pool.Deallocate(ptr, allocSize);
		}
	});

	AddTest("Allocation With A Clamped Block Size", [this](auto& ls)
	{
		struct Case
		{
			size_t requested;
			size_t aligned;
		};

		const Case cases[] = {{32, 32}, {24, 32}, {100, 112}};
		constexpr size_t blockCount = 64;
		constexpr size_t allocSize = 16;

		for (const Case& testCase : cases)
		{
			PoolAllocator pool("TestPoolAllocatorClamped", testCase.requested, blockCount);

			if (pool.GetBlockSize() != testCase.aligned)
			{
				ls << "blockSize " << testCase.requested << " should round up to " << testCase.aligned << ", but "
				   << pool.GetBlockSize() << " was used." << lferr;

				return;
			}

			for (size_t round = 0; round < 2; ++round)
			{
				Pointer blocks[blockCount] = {};
				for (size_t i = 0; i < blockCount; ++i)
				{
					blocks[i] = pool.Allocate(allocSize);
					if (blocks[i] == nullptr)
					{
						ls << "blockSize " << testCase.requested << ": round " << round << " found no free block at "
						   << i << "." << lferr;

						return;
					}

					for (size_t j = 0; j < i; ++j)
					{
						if (blocks[j] == blocks[i])
						{
							ls << "blockSize " << testCase.requested << ": round " << round << " handed block " << i
							   << " the same address as block " << j << "." << lferr;

							return;
						}
					}

					if (!OS::CheckAligned(blocks[i]))
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

	AddTest("Reallocation After An Exhausted Free List", [this](auto& ls)
	{
		constexpr size_t blockCount = 8;
		constexpr size_t returnedCount = blockCount / 2;
		constexpr size_t probedCount = returnedCount + 1;
		constexpr size_t allocSize = 16;

		PoolAllocator pool("TestPoolAllocatorExhausted", 128, blockCount);

		Pointer blocks[blockCount] = {};
		for (size_t i = 0; i < blockCount; ++i)
		{
			blocks[i] = pool.Allocate(allocSize);
			if (blocks[i] == nullptr)
			{
				ls << "exhaustion: block " << i << " was not available although the pool holds " << blockCount
				   << " and none was taken before." << lferr;

				return;
			}
		}

		if (pool.GetAvailableBlocks() != 0)
		{
			ls << "exhaustion: " << pool.GetAvailableBlocks() << " blocks report free after all " << blockCount
			   << " were allocated." << lferr;

			return;
		}

		for (size_t i = 0; i < returnedCount; ++i)
		{
			pool.Deallocate(blocks[i], allocSize);
			blocks[i] = nullptr;
		}

		Pointer reacquired[probedCount] = {};
		for (size_t i = 0; i < probedCount; ++i)
		{
			auto ptr = pool.Allocate(allocSize);
			if (ptr == nullptr)
			{
				continue;
			}

			for (size_t j = returnedCount; j < blockCount; ++j)
			{
				if (blocks[j] == ptr)
				{
					ls << "exhaustion: allocation " << i << " of the " << returnedCount << " returned blocks handed "
					   << ptr << ", the address still held by block " << j << " " << blocks[j] << "." << lferr;

					return;
				}
			}

			for (size_t j = 0; j < i; ++j)
			{
				if (reacquired[j] == ptr)
				{
					ls << "exhaustion: allocation " << i << " handed " << ptr << ", the address allocation " << j
					   << " already holds." << lferr;

					return;
				}
			}

			reacquired[i] = ptr;
		}

		for (size_t i = returnedCount; i < blockCount; ++i)
		{
			pool.Deallocate(blocks[i], allocSize);
		}

		for (const auto ptr : reacquired)
		{
			if (ptr != nullptr)
			{
				pool.Deallocate(ptr, allocSize);
			}
		}

		if (pool.GetAvailableBlocks() != blockCount)
		{
			ls << "exhaustion: " << pool.GetAvailableBlocks() << " of " << blockCount
			   << " blocks report free after every block came back." << lferr;

			return;
		}

		ls << "A pool emptied and refilled by a strict subset never re-joins a block still handed out." << lf;
	});
}
} // namespace hbe

#endif //TEST_ENABLED
