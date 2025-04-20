// Created by mooming.go@gmail.com

#pragma once

#include "AllocatorID.h"
#include "Config/BuildConfig.h"
#include "MemoryManager.h"
#include "OSAL/OSMemory.h"
#include "System/Debug.h"
#include <cstddef>
#include <cstdint>

namespace HE
{
    template <size_t Capacity>
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
        InlineMonotonicAllocator(const char* name)
            : id(InvalidAllocatorID),
              parentID(InvalidAllocatorID),
              cursor(0)
        {
            Assert(OS::CheckAligned(buffer));
            buffer[0] = 0;

            auto allocFunc = [this](size_t n) -> void* { return Allocate(n); };

            auto deallocFunc = [this](void* ptr, size_t size) {
                Deallocate(ptr, size);
            };

            auto& mmgr = MemoryManager::GetInstance();
            parentID = mmgr.GetCurrentAllocatorID();

            id = mmgr.Register(name, true, Capacity, allocFunc, deallocFunc);
        }

        ~InlineMonotonicAllocator()
        {
            auto& mmgr = MemoryManager::GetInstance();

#ifdef __MEMOR_STATISTICS__
            mmgr.ReportDeallocation(id, buffer, cursor, cursor);
#endif // __MEMOR_STATISTICS__

            mmgr.Deregister(GetID());
        }

        TPointer Allocate(size_t requested)
        {
            auto size = OS::GetAligned(requested, Config::DefaultAlign);

            const auto freeSize = GetAvailable();
            if (unlikely(size > freeSize))
            {
                auto& mmgr = MemoryManager::GetInstance();
                mmgr.LogWarning([size, freeSize](auto& ls) {
                    ls << "The requested size " << size
                       << " is exceeding its limit, " << freeSize << '.';
                });

                auto ptr = mmgr.AllocateBytes(parentID, requested);

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
                mmgr.Log(ELogLevel::Info, [this, &mmgr, ptr, size](auto& ls) {
                    ls << mmgr.GetName(id) << '[' << static_cast<int>(GetID())
                       << "]: Allocate " << static_cast<void*>(ptr)
                       << ", size = " << size;
                });
            }
#endif // __MEMORY_LOGGING__

            return ptr;
        }

        void Deallocate(const TPointer ptr, TSize requested)
        {
            auto& mmgr = MemoryManager::GetInstance();

            if (unlikely(!IsMine(ptr)))
            {
                mmgr.DeallocateBytes(parentID, ptr, requested);
                return;
            }

#if __MEMORY_LOGGING__
            mmgr.Log(ELogLevel::Info, [this, &mmgr, ptr, requested](auto& ls) {
                ls << mmgr.GetName(id) << '[' << static_cast<int>(GetID())
                   << "] Deallocate call shall be ignored. ptr = "
                   << static_cast<void*>(ptr) << ", size = " << requested;
            });
#endif // __MEMORY_LOGGING__

#ifdef PROFILE_ENABLED
            mmgr.ReportDeallocation(id, ptr, requested, 0);
#endif // PROFILE_ENABLED
        }

        size_t GetAvailable() const
        {
            Assert(Capacity >= cursor);
            return Capacity - cursor;
        }

        size_t GetUsage() const
        {
            Assert(cursor < Capacity);
            return cursor;
        }

        inline auto GetID() const { return id; }

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
} // namespace HE

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace HE
{

    class InlineMonotonicAllocatorTest : public TestCollection
    {
    public:
        InlineMonotonicAllocatorTest()
            : TestCollection("InlineMonotonicAllocatorTest")
        {
        }

    protected:
        virtual void Prepare() override;
    };

} // namespace HE
#endif //__UNIT_TEST__
