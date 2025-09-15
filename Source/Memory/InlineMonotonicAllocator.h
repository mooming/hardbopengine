// Created by mooming.go@gmail.com

#pragma once

#include <cstddef>
#include <cstdint>
#include "AllocatorID.h"
#include "Config/BuildConfig.h"
#include "Core/Debug.h"
#include "MemoryManager.h"
#include "OSAL/OSMemory.h"

namespace hbe
{
	template<size_t Capacity>
	class InlineMonotonicAllocator final
	{
	public:
		using TSize = size_t;
		using TPointer = void*;

	private:
		TAllocatorID id;
		TAllocatorID parentID;
		TSize cursor;
		ALIGN uint8_t buffer[Capacity];

	public:
		explicit InlineMonotonicAllocator(const char* name) :
			id(InvalidAllocatorID), parentID(InvalidAllocatorID), cursor(0), buffer{}
		{
			Assert(OS::CheckAligned(buffer));
			buffer[0] = 0;

			auto allocFunc = [](void* allocatorPtr, size_t n) -> void*
			{
				auto allocator = static_cast<InlineMonotonicAllocator*>(allocatorPtr);
				return allocator->Allocate(n);
			};

			auto deallocFunc = [](void* allocatorPtr, void* ptr, size_t size)
			{
				auto allocator = static_cast<InlineMonotonicAllocator*>(allocatorPtr);
				allocator->Deallocate(ptr, size);
			};

			parentID = hbe::MemoryManager::GetCurrentAllocatorID();
			auto& mmgr = MemoryManager::GetInstance();
			id = mmgr.RegisterAllocator(this, name, true, Capacity, allocFunc, deallocFunc);
		}

		~InlineMonotonicAllocator()
		{
			auto& mmgr = MemoryManager::GetInstance();

#ifdef __MEMOR_STATISTICS__
			mmgr.ReportDeallocation(id, buffer, cursor, cursor);
#endif // __MEMOR_STATISTICS__

			mmgr.DeregisterAllocator(GetID());
		}

		TPointer Allocate(size_t requested)
		{
			auto size = OS::GetAligned(requested, Config::DefaultAlign);

			const auto freeSize = GetAvailable();
			if (unlikely(size > freeSize))
			{
				auto& mmgr = MemoryManager::GetInstance();
				mmgr.LogWarning([size, freeSize](auto& ls)
				{ ls << "The requested size " << size << " is exceeding its limit, " << freeSize << '.'; });

				auto ptr = mmgr.Allocate(parentID, requested);

				return ptr;
			}

			auto ptr = reinterpret_cast<void*>(buffer + cursor);
			cursor += size;

#ifdef __MEMOR_STATISTICS__
			{
				auto& mmgr = MemoryManager::GetInstance();
				mmgr.ReportAllocation(id, ptr, size, size);
			}
#endif // __MEMOR_STATISTICS__

#if __MEMORY_LOGGING__
			{
				auto& mmgr = MemoryManager::GetInstance();
				mmgr.Log(ELogLevel::Info, [this, &mmgr, ptr, size](auto& ls)
				{
					ls << mmgr.GetAllocatorName(id) << '[' << static_cast<int>(GetID()) << "]: Allocate "
					   << static_cast<void*>(ptr) << ", size = " << size;
				});
			}
#endif // __MEMORY_LOGGING__

			return ptr;
		}

		void Deallocate(TPointer ptr, TSize requested)
		{
			auto& mmgr = MemoryManager::GetInstance();

			if (unlikely(!IsMine(ptr)))
			{
				mmgr.Deallocate(parentID, ptr, requested);
				return;
			}

#if __MEMORY_LOGGING__
			mmgr.Log(ELogLevel::Info, [this, &mmgr, ptr, requested](auto& ls)
			{
				ls << mmgr.GetAllocatorName(id) << '[' << static_cast<int>(GetID())
				   << "] Deallocate call shall be ignored. ptr = " << static_cast<void*>(ptr)
				   << ", size = " << requested;
			});
#endif // __MEMORY_LOGGING__

#if PROFILE_ENABLED
			mmgr.ReportDeallocation(id, ptr, requested, 0);
#endif // PROFILE_ENABLED
		}

		[[nodiscard]] size_t GetAvailable() const
		{
			Assert(Capacity >= cursor);
			return Capacity - cursor;
		}

		[[nodiscard]] size_t GetUsage() const
		{
			Assert(cursor < Capacity);
			return cursor;
		}

		auto GetID() const { return id; }

	private:
		bool IsMine(TPointer ptr) const
		{
			auto bytePtr = reinterpret_cast<const uint8_t*>(ptr);
			if (bytePtr < buffer)
			{
				return false;
			}

			if (bytePtr >= (buffer + Capacity))
			{
				return false;
			}

			return true;
		}
	};
} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{

	class InlineMonotonicAllocatorTest : public TestCollection
	{
	public:
		InlineMonotonicAllocatorTest() : TestCollection("InlineMonotonicAllocatorTest") {}

	protected:
		void Prepare() override;
	};

} // namespace hbe
#endif //__UNIT_TEST__
