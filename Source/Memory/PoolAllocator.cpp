// Created by mooming.go@gmail.com

#include "PoolAllocator.h"

#include "MemoryManager.h"
#include "OSAL/OSMemory.h"
#include "System/CommonUtil.h"
#include "System/Debug.h"

namespace hbe
{
#if PROFILE_ENABLED
	PoolAllocator::PoolAllocator(const char* name, TSize inBlockSize, TSize numberOfBlocks,
								 const hbe::source_location location)
#else // PROFILE_ENABLED
	PoolAllocator::PoolAllocator(const char* name, TSize inBlockSize, TSize numberOfBlocks)
#endif // PROFILE_ENABLED
		:
		id(InvalidAllocatorID), parentID(InvalidAllocatorID), name(name),
		blockSize(OS::GetAligned(std::max(inBlockSize, sizeof(TSize)), sizeof(TSize))), numberOfBlocks(numberOfBlocks),
		numberOfFreeBlocks(numberOfBlocks), buffer(nullptr)
#if PROFILE_ENABLED
		,
		maxUsedBlocks(0), srcLocation(location)
#endif // PROFILE_ENABLED
	{
		Assert(blockSize >= sizeof(TSize));

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

		id = mmgr.RegisterAllocator(this, name, false, totalSize, allocFunc, deallocFunc);
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
		mmgr.Deallocate(buffer, totalSize);

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
			WriteNextIndex(ptr, numberOfBlocks - 1);
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
		Assert(index <= numberOfBlocks, "PoolAllocator: out of bounds index = %u / %u", index, numberOfBlocks);

		return index;
	}

	void PoolAllocator::WriteNextIndex(Pointer ptr, TSize index)
	{
		Assert(index < numberOfBlocks, "PoolAllocator: out of bounds index. The index ", index, " should be less than ",
			   numberOfBlocks);

		SetAs<TSize>(ptr, index);
	}

	Pointer PoolAllocator::AllocateBlock()
	{
		if (!availables)
		{
			auto& mmgr = MemoryManager::GetInstance();
			mmgr.LogError([this](auto& ls)
			{
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

#ifdef __UNIT_TEST__

namespace hbe
{

	void PoolAllocatorTest::Prepare()
	{
		AddTest("Construction", [](auto&)
		{
			for (int i = 1; i < 100; ++i)
			{
				PoolAllocator pool("TestPoolAllocator", i, 100);
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
					ls << "The size is incorrect. " << size << ", but 100 expected." << lferr;
					break;
				}

				pool.Deallocate(ptr, allocSize);
			}
		});
	}

} // namespace hbe

#endif //__UNIT_TEST__
