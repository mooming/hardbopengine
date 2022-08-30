// Created by mooming.go@gmail.com, 2017

#pragma once

#include "AllocatorID.h"
#include "BaseAllocator.h"
#include "MemoryManager.h"
#include "Config/EngineConfig.h"
#include "OSAL/OSMemory.h"
#include "String/StaticString.h"
#include "System/Debug.h"
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <limits>
#include <type_traits>


namespace HE
{
template <class T, int BufferSize = 64, int NumBuffers = 2>
struct ALIGN16 InlinePoolAllocator final
{
    using value_type = T;
    using TIndex = decltype(BufferSize);

    template<typename U>
    using rebind = InlinePoolAllocator<U>;

    static_assert(NumBuffers >= 0, "The number of buffers should be greater than or equal to zero.");
    static_assert(std::is_signed<TIndex>(), "The type of BufferSize should be signed integral.");
    static_assert(BufferSize >= 0, "The buffer size should be greater than or equal to zero.");
    
private:
    bool isAllocated[NumBuffers];
    T buffer[NumBuffers][BufferSize];
    
    TAllocatorID id;
    size_t fallbackCount;
    int indexHint;

public:
    InlinePoolAllocator(const InlinePoolAllocator&) = delete;
    InlinePoolAllocator(InlinePoolAllocator&&) = delete;
    InlinePoolAllocator& operator= (const InlinePoolAllocator&) = delete;
    InlinePoolAllocator& operator= (InlinePoolAllocator&&) = delete;

public:
    InlinePoolAllocator()
        : id(InvalidAllocatorID)
        , fallbackCount(0)
        , indexHint(0)
    {
#ifdef __MEMORY_VERIFICATION__
        Assert(OS::CheckAligned(this));
#endif // __MEMORY_VERIFICATION__
        
        if (unlikely(BufferSize <= 0))
            return;
        
        for (auto& item : isAllocated)
            item = false;
        
#ifdef __MEMORY_VERIFICATION__
        constexpr size_t length = sizeof(T) * BufferSize * NumBuffers;
        memset(buffer, 0, length);
#endif // __MEMORY_VERIFICATION__
        
        RegisterAllocator();
    }
    
    ~InlinePoolAllocator()
    {
        DeregisterAllocator();
    }

    StaticString GetName() const
    {
        static StaticString name("InlinePoolAllocator");
        return name;
    }
    
    T* allocate(std::size_t n)
    {
        auto FallbackAlloc = [this, n]() -> T*
        {
            auto& mmgr = MemoryManager::GetInstance();
            constexpr size_t sizeOfT = sizeof(T);
            const auto nBytes = n * sizeOfT;
            auto ptr = reinterpret_cast<T*>(mmgr.SysAllocate(nBytes));
            ++fallbackCount;
            
            return ptr;
        };
        
        if (n > BufferSize)
            return FallbackAlloc();
        
        if (unlikely(n == 0))
            return &buffer[0][0];
        
#ifdef __MEMORY_VERIFICATION__
        Assert(BufferSize > 0);
#endif // __MEMORY_VERIFICATION__
        
        int index = indexHint;
        for (int i = 0; i < NumBuffers; ++i, ++index)
        {
            index = index >= NumBuffers ? 0 : index;
            if (isAllocated[index])
                continue;
            
            indexHint = index + 1;
            if (indexHint >= NumBuffers)
                indexHint = 0;
            
            auto ptr = &buffer[index][0];
            
#ifdef __MEMOR_STATISTICS__
            auto& mmgr = MemoryManager::GetInstance();
            constexpr size_t unit = sizeof(T);
            constexpr size_t bufferSizeBytes = BufferSize * unit;
            mmgr.ReportAllocation(id, ptr, n * unit, bufferSizeBytes);
#endif // __MEMOR_STATISTICS__
            
            return ptr;
        }
        
        return FallbackAlloc();
    }
    
    void deallocate (T* ptr, std::size_t n) noexcept
    {
        if (unlikely(ptr == nullptr || n == 0))
            return;
        
        for (int i = 0; i < NumBuffers; ++i)
        {
            if (ptr != &buffer[i][0])
                continue;

#ifdef __MEMOR_STATISTICS__
            auto& mmgr = MemoryManager::GetInstance();
            constexpr size_t unit = sizeof(T);
            constexpr size_t bufferSizeBytes = BufferSize * unit;
            mmgr.ReportDeallocation(id, ptr, n * unit, bufferSizeBytes);
#endif // __MEMOR_STATISTICS__
            
            isAllocated[i] = false;
            indexHint = i;
            
            return;
        }

        auto& mmgr = MemoryManager::GetInstance();
        constexpr size_t sizeOfT = sizeof(T);
        const auto nBytes = n * sizeOfT;
        mmgr.SysDeallocate(ptr, nBytes);
    }
    
    auto GetID() const { return id; }
    auto GetFallbackCount() const { return fallbackCount; }

    template <class U>
    bool operator==(const U&) const { return false; }
    
    template <class U>
    bool operator!=(const U&) const { return true; }
    
private:
    void RegisterAllocator()
    {
        auto& mmgr = MemoryManager::GetInstance();
        
        auto allocFunc = [this](size_t nBytes) -> void*
        {
            constexpr auto unit = sizeof(T);
            
            if (unit == 1)
            {
                return static_cast<void*>(allocate(nBytes));
            }
            else
            {
                const auto unitCounts = (nBytes + unit - 1) / unit;
                return static_cast<void*>(allocate(unitCounts));
            }
        };
        
        auto deallocFunc = [this](void* ptr, size_t nBytes)
        {
            constexpr auto unit = sizeof(T);
            if (unit == 1)
            {
                deallocate(static_cast<T*>(ptr), nBytes);
                return;
            }
            else
            {
                const auto unitCounts = (nBytes + unit - 1) / unit;
                deallocate(static_cast<T*>(ptr), unitCounts);
            }
        };
        
        const auto capacity = BufferSize * NumBuffers * sizeof(T);
        id = mmgr.Register("InlinePoolAllocator", true
            , capacity, allocFunc, deallocFunc);

        FatalAssert(id != InvalidAllocatorID);
        FatalAssert(id != 0);
    }
    
    void DeregisterAllocator()
    {
        if (id == InvalidAllocatorID)
            return;

        auto& mmgr = MemoryManager::GetInstance();
        mmgr.Deregister(id);
    }
};
} // HE

#ifdef __UNIT_TEST__
#include "Test/TestCase.h"

namespace HE
{
class InlinePoolAllocatorTest : public TestCase
{
public:
    
    InlinePoolAllocatorTest()
    : TestCase("InlinePoolAllocatorTest")
    {
    }
    
protected:
    virtual bool DoTest() override;
};
} // HE
#endif //__UNIT_TEST__
