// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

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
		HE_ALIGN uint8_t buffer[Capacity];

	public:
		explicit InlineMonotonicAllocator(const char* name) :
			id(InvalidAllocatorID), parentID(InvalidAllocatorID), cursor(0), buffer{}
		{
			Assert(OS::checkAligned(buffer));
			buffer[0] = 0;

			auto allocFunc = [](void* allocatorPtr, size_t n) -> void*
			{
				auto allocator = static_cast<InlineMonotonicAllocator*>(allocatorPtr);
				return allocator->allocate(n);
			};

			auto deallocFunc = [](void* allocatorPtr, void* ptr, size_t size)
			{
				auto allocator = static_cast<InlineMonotonicAllocator*>(allocatorPtr);
				allocator->Deallocate(ptr, size);
			};

			parentID = hbe::MemoryManager::getCurrentAllocatorID();
			auto& mmgr = MemoryManager::getInstance();
			id = mmgr.registerAllocator(this, name, true, Capacity, allocFunc, deallocFunc);
		}

		~InlineMonotonicAllocator()
		{
			auto& mmgr = MemoryManager::getInstance();

#ifdef __MEMOR_STATISTICS__
			mmgr.reportDeallocation(id, buffer, cursor, cursor);
#endif // __MEMOR_STATISTICS__

			mmgr.deregisterAllocator(getID());
		}

		[[nodiscard]] TPointer allocate(size_t requested)
		{
			auto size = OS::getAligned(requested, Config::DefaultAlign);

			const auto freeSize = getAvailable();
			if (unlikely(size > freeSize))
			{
				auto& mmgr = MemoryManager::getInstance();
				mmgr.logWarning([size, freeSize](auto& ls)
				{ ls << "The requested size " << size << " is exceeding its limit, " << freeSize << '.'; });

				auto ptr = mmgr.allocate(parentID, requested);

				return ptr;
			}

			auto ptr = reinterpret_cast<void*>(buffer + cursor);
			cursor += size;

#ifdef __MEMOR_STATISTICS__
			{
				auto& mmgr = MemoryManager::getInstance();
				mmgr.reportAllocation(id, ptr, size, size);
			}
#endif // __MEMOR_STATISTICS__

#if MEMORY_LOGGING_ENABLED
			{
				auto& mmgr = MemoryManager::getInstance();
				mmgr.log(ELogLevel::Info, [this, &mmgr, ptr, size](auto& ls)
				{
					ls << mmgr.getAllocatorName(id) << '[' << static_cast<int>(getID()) << "]: Allocate "
					   << static_cast<void*>(ptr) << ", size = " << size;
				});
			}
#endif // MEMORY_LOGGING_ENABLED

			return ptr;
		}

		void Deallocate(TPointer ptr, TSize requested) noexcept
		{
			auto& mmgr = MemoryManager::getInstance();

			if (unlikely(!isMine(ptr)))
			{
				mmgr.Deallocate(parentID, ptr, requested);
				return;
			}

#if MEMORY_LOGGING_ENABLED
			mmgr.log(ELogLevel::Info, [this, &mmgr, ptr, requested](auto& ls)
			{
				ls << mmgr.getAllocatorName(id) << '[' << static_cast<int>(getID())
				   << "] Deallocate call shall be ignored. ptr = " << static_cast<void*>(ptr)
				   << ", size = " << requested;
			});
#endif // MEMORY_LOGGING_ENABLED

#if PROFILE_ENABLED
			mmgr.reportDeallocation(id, ptr, requested, 0);
#endif // PROFILE_ENABLED
		}

		[[nodiscard]] size_t getAvailable() const
		{
			Assert(Capacity >= cursor);
			return Capacity - cursor;
		}

		[[nodiscard]] size_t getUsage() const
		{
			Assert(cursor < Capacity);
			return cursor;
		}

		[[nodiscard]] auto getID() const { return id; }

	private:
		[[nodiscard]] bool isMine(TPointer ptr) const
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
		void prepare() noexcept override;
	};

} // namespace hbe
#endif //__UNIT_TEST__
