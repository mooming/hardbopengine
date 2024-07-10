// Created by mooming.go@gmail.com

#include "MonotonicAllocator.h"

#include "Config/BuildConfig.h"
#include "Config/EngineSettings.h"
#include "MemoryManager.h"
#include "System/Debug.h"


using namespace HE;

MonotonicAllocator::MonotonicAllocator(const char* name, TSize inCapacity)
    : id(InvalidAllocatorID), cursor(0), capacity(inCapacity), buffer(nullptr)
{
    {
        constexpr auto AlignUnit = Config::DefaultAlign;
        const auto multiplier = (capacity + AlignUnit - 1) / AlignUnit;
        capacity = multiplier * AlignUnit;
    }

    auto& mmgr = MemoryManager::GetInstance();
    parentID = mmgr.GetCurrentAllocatorID();
    bufferPtr = mmgr.AllocateBytes(capacity);

    auto allocFunc = [this](size_t n) -> void* { return Allocate(n); };

    auto deallocFunc = [this](void* ptr, size_t size) { Deallocate(ptr, size); };

    id = mmgr.Register(name, false, capacity, allocFunc, deallocFunc);
}

MonotonicAllocator::~MonotonicAllocator()
{
    auto& mmgr = MemoryManager::GetInstance();

#ifdef PROFILE_ENABLED
    mmgr.ReportDeallocation(id, bufferPtr, 0, cursor);
#endif // PROFILE_ENABLED

    mmgr.DeallocateBytes(bufferPtr, capacity);
    mmgr.Deregister(GetID());
}

void* MonotonicAllocator::Allocate(const size_t requested)
{
    size_t size = requested;

    {
        constexpr auto AlignUnit = Config::DefaultAlign;
        const auto multiplier = (size + AlignUnit - 1) / AlignUnit;
        size = multiplier * AlignUnit;
    }

    const auto freeSize = GetAvailable();
    if (unlikely(size > freeSize))
    {
        auto& mmgr = MemoryManager::GetInstance();
        mmgr.LogWarning(
            [size, freeSize](auto& ls) {
                ls << "The requested size " << size << " is exceeding its limit, " << freeSize
                   << '.';
            });

        auto ptr = mmgr.AllocateBytes(parentID, requested);

        return ptr;
    }

    auto ptr = reinterpret_cast<void*>(buffer + cursor);
    cursor += size;

#ifdef PROFILE_ENABLED
    {
        auto& mmgr = MemoryManager::GetInstance();
        mmgr.ReportAllocation(id, ptr, requested, size);
    }
#endif // PROFILE_ENABLED


    return ptr;
}

void MonotonicAllocator::Deallocate(const Pointer ptr, TSize requested)
{
    auto& mmgr = MemoryManager::GetInstance();

    if (unlikely(!IsMine(ptr)))
    {
        mmgr.DeallocateBytes(parentID, ptr, requested);
        return;
    }

#ifdef __MEMORY_LOGGING__
    mmgr.Log(
        ELogLevel::Verbose,
        [this, &mmgr, ptr, requested](auto& lout)
        {
            lout << mmgr.GetName(id) << '[' << static_cast<int>(GetID())
                 << "] Deallocate call shall be ignored. ptr = " << static_cast<void*>(ptr)
                 << ", requested size = " << requested;
        });
#endif // __MEMORY_LOGGING__

#ifdef PROFILE_ENABLED
    {
        auto& mmgr = MemoryManager::GetInstance();
        mmgr.ReportDeallocation(id, ptr, requested, 0);
    }
#endif // PROFILE_ENABLED
}

size_t MonotonicAllocator::GetAvailable() const
{
    Assert(capacity >= cursor);
    return capacity - cursor;
}

size_t MonotonicAllocator::GetUsage() const
{
    Assert(cursor < capacity);
    return cursor;
}

bool MonotonicAllocator::IsMine(const TPointer ptr) const
{
    auto bytePtr = reinterpret_cast<const uint8_t*>(ptr);
    if (bytePtr < buffer)
    {
        return false;
    }

    if (bytePtr >= (buffer + capacity))
    {
        return false;
    }

    return true;
}

#ifdef __UNIT_TEST__
#include "HSTL/HVector.h"
#include "String/String.h"


void MonotonicAllocatorTest::Prepare()
{
    using namespace std;
    using namespace HSTL;

    AddTest(
        "Vector Allocation",
        [this](auto& ls)
        {
            MonotonicAllocator alloc("Test::MonotonicAllocator", 1024 * 1024);

            {
                AllocatorScope scope(alloc.GetID());

                HVector<int> a;
                a.push_back(0);
            }

            if (alloc.GetUsage() == 0)
            {
                ls << "Monotonic Allocator doesn't provide deallocation."
                   << " Usage should not be zero, but " << alloc.GetUsage() << lferr;
            }
        });

    AddTest(
        "Allocation (2)",
        [this](auto& ls)
        {
            MonotonicAllocator alloc("Test::MonotonicAllocator", 1024 * 1024);

            {
                AllocatorScope scope(alloc.GetID());

                HVector<int> a;
                a.push_back(0);

                HVector<int> b;
                b.push_back(1);
            }

            if (alloc.GetUsage() == 0)
            {
                ls << "Monotonic Allocator doesn't provide deallocation."
                   << " Usage should not be zero, but " << alloc.GetUsage() << lferr;
            }
        });

    AddTest(
        "Deallocation",
        [this](auto& ls)
        {
            MonotonicAllocator alloc("Test::MonotonicAllocator", 1024);
            AllocatorScope scope(alloc.GetID());

            {
                String a = "0";
            }

            if (alloc.GetUsage() == 0)
            {
                ls << "Monotonic Allocator doesn't provide deallocation."
                   << " Usage should not be zero, but " << alloc.GetUsage() << lferr;
            }
        });

    AddTest(
        "Deallocation (2)",
        [this](auto& ls)
        {
            MonotonicAllocator alloc("Test::MonotonicAllocator", 1024);
            AllocatorScope scope(alloc.GetID());

            {
                String a = "0";
                String b = "1";
            }

            if (alloc.GetUsage() == 0)
            {
                ls << "Monotonic Allocator doesn't provide deallocation."
                   << " Usage should not be zero, but " << alloc.GetUsage() << lferr;
            }
        });
}

#endif //__UNIT_TEST__
