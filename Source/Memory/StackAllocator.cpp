// Created by mooming.go@gmail.com, 2017

#include "StackAllocator.h"

#include "AllocatorScope.h"
#include "MemoryManager.h"
#include "Config/EngineSettings.h"
#include "OSAL/Intrinsic.h"
#include "System/Debug.h"
#include "System/Exception.h"
#include "System/CommonUtil.h"


using namespace HE;

#ifdef PROFILE_ENABLED
StackAllocator::StackAllocator(const char* name
    , SizeType inCapacity, const TSrcLoc location)
    : id(InvalidAllocatorID)
    , capacity(inCapacity)
    , cursor(0)
    , buffer(nullptr)
    , srcLocation(location)
#else // PROFILE_ENABLED
StackAllocator::StackAllocator(const char* name, SizeType inCapacity)
    : id(InvalidAllocatorID)
    , capacity(inCapacity)
    , cursor(0)
    , buffer(nullptr)
#endif // PROFILE_ENABLED
{
    {
        constexpr auto AlignUnit = Config::DefaultAlign;
        const auto multiplier = (capacity + AlignUnit - 1) / AlignUnit;
        capacity = multiplier * AlignUnit;
    }
    
    auto& mmgr = MemoryManager::GetInstance();
    bufferPtr = mmgr.AllocateBytes(capacity);
    
    auto allocFunc = [this](size_t n) -> void*
    {
        return Allocate(n);
    };
    
    auto deallocFunc = [this](void* ptr, size_t size)
    {
        Deallocate(ptr, size);
    };
    
    id = mmgr.Register(name, false, capacity, allocFunc, deallocFunc);
}

StackAllocator::~StackAllocator()
{
    auto& mmgr = MemoryManager::GetInstance();
    mmgr.DeallocateBytes(bufferPtr, capacity);
    
#ifdef PROFILE_ENABLED
    mmgr.Deregister(GetID(), srcLocation);
#else // PROFILE_ENABLED
    mmgr.Deregister(GetID());
#endif // PROFILE_ENABLED
}

void *StackAllocator::Allocate (size_t size)
{
    {
        constexpr auto AlignUnit = Config::DefaultAlign;
        const auto multiplier = (size + AlignUnit - 1) / AlignUnit;
        size = multiplier * AlignUnit;
    }

    const auto freeSize = GetAvailable();
    if (unlikely(size > freeSize))
    {
        auto& mmgr = MemoryManager::GetInstance();
        mmgr.LogError([this, size, freeSize](auto& lout){
            lout << "StackAllocator: Not Enough Memory! "
                << "require = " << size << " : free = "
                << freeSize << " / " << capacity;
        });

        return nullptr;
    }

    auto ptr = reinterpret_cast<void*>(buffer + cursor);
    cursor += size;

#ifdef PROFILE_ENABLED
    {
        auto& mmgr = MemoryManager::GetInstance();
        mmgr.ReportAllocation(id, ptr, size, size);
    }
#endif // PROFILE_ENABLED

    return ptr;
}

void StackAllocator::Deallocate(const Pointer ptr, SizeType size)
{
    Assert(IsMine(ptr));

    {
        constexpr auto AlignUnit = Config::DefaultAlign;
        const auto multiplier = (size + AlignUnit - 1) / AlignUnit;
        size = multiplier * AlignUnit;
    }

    Assert(cursor >= size);

    auto expected = buffer + cursor;
    auto provided = reinterpret_cast<Byte*>(ptr) + size;
    if (unlikely(expected != provided))
    {
        auto& mmgr = MemoryManager::GetInstance();
        mmgr.LogError([this, ptr, expected, provided](auto& lout)
        {
            lout << "StackAllocator[" << static_cast<int>(GetID())
                << "]: Pointer mismatched! ptr = " << ptr << ", "
                << static_cast<void*>(expected) << " is expected. But "
                << static_cast<void*>(provided) << " is provided.";
        });

        Assert(false);
        return;
    }

    cursor -= size;

#ifdef PROFILE_ENABLED
    {
        auto& mmgr = MemoryManager::GetInstance();
        mmgr.ReportDeallocation(id, ptr, size, size);
    }
#endif // PROFILE_ENABLED
}

size_t StackAllocator::GetAvailable() const
{
    Assert(capacity >= cursor);
    return capacity - cursor;
}

size_t StackAllocator::GetUsage() const
{
    Assert(cursor < capacity);
    return cursor;
}

bool StackAllocator::IsMine(Pointer ptr) const
{
    if (ptr < static_cast<void*>(buffer))
        return false;
    
    if (ptr >= static_cast<void*>(buffer + capacity))
        return false;
    
    return true;
}

#ifdef __UNIT_TEST__
#include "ScopedAllocator.h"
#include "HSTL/HVector.h"
#include "String/String.h"


void StackAllocatorTest::Prepare()
{
    using namespace std;
    using namespace HSTL;
    
    AddTest("Vector Allocation", [this](auto& ls)
    {
        StackAllocator stack("Test::StackAllocator", 1024 * 1024);

        {
            AllocatorScope scope(stack.GetID());

            HVector<int> a;
            a.push_back(0);
        }

        if (stack.GetUsage() != 0)
        {
            ls << "Deallocation Failed. Usage should be zero, but "
                << stack.GetUsage() << lferr;
        }
    });
    
    AddTest("Allocation (2)", [this](auto& ls)
    {
        StackAllocator stack("Test::StackAllocator::Allocation (2)", 1024 * 1024);

        {
            AllocatorScope scope(stack.GetID());

            HVector<int> a;
            a.push_back(0);

            HVector<int> b;
            b.push_back(1);
        }

        if (stack.GetUsage() != 0)
        {
            ls << "Deallocation Failed. Usage should be zero, but "
                << stack.GetUsage() << lferr;
        }
    });
    
    
    AddTest("Deallocation", [this](auto& ls)
    {
        StackAllocator stack("Test::StackAllocator::Deallocation", 1024 * 1024);
        AllocatorScope scope(stack.GetID());
        
        {
            String a = "0";
        }
        
        if (stack.GetUsage() != 0)
        {
            ls << "Deallocation Failed. Usage should be zero, but "
                << stack.GetUsage() << lferr;
        }
    });
    
    AddTest("Deallocation (2)", [this](auto& ls)
    {
        StackAllocator stack("Test::StackAllocator", 1024 * 1024);
        AllocatorScope scope(stack.GetID());
        
        {
            String a = "0";
            String b = "1";
        }
        
        if (stack.GetUsage() != 0)
        {
            ls << "Deallocation Failed. Usage should be zero, but "
                << stack.GetUsage() << lferr;
        }
    });

    AddTest("Nested Usage", [this](auto& ls)
    {
        using TAlloc = StackAllocator;
        int depthSeed = 0;

        ScopedAllocator<TAlloc> scope0("NestedStack0", 1024);

        const auto depth = depthSeed++;
        ls << "Neted Level " << depth << ", free size = "
            << scope0.GetAllocator().GetAvailable() << lf;

        {
            ScopedAllocator<TAlloc> scope1("NestedStack1", 512);

            auto ptr = New<long double>(0);

            const auto depth = depthSeed++;
            ls << "Neted Level " << depth << ", free size = "
                << scope1.GetAllocator().GetAvailable() << " / "
                << scope0.GetAllocator().GetAvailable() << lf;

            {
                ScopedAllocator<TAlloc> scope2("NestedStack2", 256);

                auto ptr = New<long double>(0);

                const auto depth = depthSeed++;
                ls << "Neted Level " << depth << ", free size = "
                    << scope2.GetAllocator().GetAvailable() << " / "
                    << scope1.GetAllocator().GetAvailable() << " / "
                    << scope0.GetAllocator().GetAvailable() << lf;

                {
                    ScopedAllocator<TAlloc> scope3("NestedStack3", 128);

                    auto ptr = New<long double>(0);

                    const auto depth = depthSeed++;
                    ls << "Neted Level " << depth << ", free size = "
                        << scope3.GetAllocator().GetAvailable() << " / "
                        << scope2.GetAllocator().GetAvailable() << " / "
                        << scope1.GetAllocator().GetAvailable() << " / "
                        << scope0.GetAllocator().GetAvailable() << lf;

                    {
                        ScopedAllocator<TAlloc> scope4("NestedStack4", 64);

                        auto ptr = New<long double>(0);

                        const auto depth = depthSeed++;
                        ls << "Neted Level " << depth << ", free size = "
                            << scope4.GetAllocator().GetAvailable() << " / "
                            << scope3.GetAllocator().GetAvailable() << " / "
                            << scope2.GetAllocator().GetAvailable() << " / "
                            << scope1.GetAllocator().GetAvailable() << " / "
                            << scope0.GetAllocator().GetAvailable() << lf;

                        {
                            ScopedAllocator<TAlloc> scope5("NestedStack05", 32);

                            auto ptr = New<long double>(0);

                            const auto depth = depthSeed++;
                            ls << "Neted Level " << depth << ", free size = "
                                << scope5.GetAllocator().GetAvailable() << " / "
                                << scope4.GetAllocator().GetAvailable() << " / "
                                << scope3.GetAllocator().GetAvailable() << " / "
                                << scope2.GetAllocator().GetAvailable() << " / "
                                << scope1.GetAllocator().GetAvailable() << " / "
                                << scope0.GetAllocator().GetAvailable() << lf;

                            Delete(ptr);
                        }

                        Delete(ptr);

                        ls << "Neted Level " << depth << ", free size = "
                            << scope4.GetAllocator().GetAvailable() << " / "
                            << scope3.GetAllocator().GetAvailable() << " / "
                            << scope2.GetAllocator().GetAvailable() << " / "
                            << scope1.GetAllocator().GetAvailable() << " / "
                            << scope0.GetAllocator().GetAvailable() << lf;
                    }

                    Delete(ptr);

                    ls << "Neted Level " << depth << ", free size = "
                        << scope3.GetAllocator().GetAvailable() << " / "
                        << scope2.GetAllocator().GetAvailable() << " / "
                        << scope1.GetAllocator().GetAvailable() << " / "
                        << scope0.GetAllocator().GetAvailable() << lf;
                }

                Delete(ptr);

                ls << "Neted Level " << depth << ", free size = "
                    << scope2.GetAllocator().GetAvailable() << " / "
                    << scope1.GetAllocator().GetAvailable() << " / "
                    << scope0.GetAllocator().GetAvailable() << lf;
            }

            Delete(ptr);

            ls << "Neted Level " << depth << ", free size = "
                << scope1.GetAllocator().GetAvailable() << " / "
                << scope0.GetAllocator().GetAvailable() << lf;
        }

        ls << "Neted Level " << depth << ", free size = "
            << scope0.GetAllocator().GetAvailable() << lf;
    });
}

#endif //__UNIT_TEST__
