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


namespace HE
{
template <class T, int BufferSize, int NumBuffers = 2>
struct ALIGNED InlinePoolAllocator final
{
    using TIndex = decltype(BufferSize);

    using value_type = T;

    template <class U>
    struct rebind {
        typedef InlinePoolAllocator<U, BufferSize, NumBuffers> other;
    };
 
    static_assert(NumBuffers > 0, "The number of buffers should be greater than or equal to zero.");
    static_assert(std::is_signed<TIndex>(), "The type of BufferSize should be signed integral.");
    static_assert(BufferSize > 0, "The buffer size should be greater than or equal to zero.");
    
private:
    bool isAllocated[NumBuffers];
    uint8_t buffer[NumBuffers][BufferSize * sizeof(T)];
    
    TAllocatorID id;
    TAllocatorID parentID;
    size_t fallbackCount;
    int indexHint;

public:
    InlinePoolAllocator()
        : id(InvalidAllocatorID)
        , parentID(InvalidAllocatorID)
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

    InlinePoolAllocator(TAllocatorID inParentID)
        : id(InvalidAllocatorID)
        , parentID(inParentID)
        , fallbackCount(0)
        , indexHint(0)
    {
    }

    InlinePoolAllocator(const InlinePoolAllocator&)
        : InlinePoolAllocator()
    {
    }
    
    ~InlinePoolAllocator()
    {
        DeregisterAllocator();
    }

    template <typename U>
    operator InlinePoolAllocator<U, BufferSize, NumBuffers> ()
    {
        using TCastedAlloc = InlinePoolAllocator<U, BufferSize, NumBuffers>;

        if (parentID != InvalidAllocatorID)
        {
            return TCastedAlloc(parentID);
        }

        return TCastedAlloc(GetID());
    }

    StaticString GetName() const
    {
        static StaticString name("InlinePoolAllocator");
        return name;
    }
    
    T* allocate(std::size_t n)
    {
        constexpr size_t unit = sizeof(T);
        const auto nBytes = n * unit;

        if (parentID == InvalidAllocatorID)
        {
            auto ptr = AllocateBytes(nBytes);
            return reinterpret_cast<T*>(ptr);
        }

        auto& mmgr = MemoryManager::GetInstance();
        auto ptr = mmgr.Allocate(parentID, nBytes);

        return reinterpret_cast<T*>(ptr);
    }
    
    void deallocate (T* ptr, std::size_t n) noexcept
    {
        constexpr size_t unit = sizeof(T);
        const auto nBytes = n * unit;

        void* voidPtr = reinterpret_cast<void*>(ptr);

        if (parentID == InvalidAllocatorID)
        {
            return DeallocateBytes(voidPtr, nBytes);
        }

        auto& mmgr = MemoryManager::GetInstance();
        mmgr.Deallocate(parentID, voidPtr, nBytes);
    }
    
    auto GetID() const { return id; }
    auto GetFallbackCount() const { return fallbackCount; }
    auto GetBlockSize() const { return BufferSize; }
    auto GetNumBlocks() const { return NumBuffers; }

    bool operator==(const InlinePoolAllocator&) const
    {
        return false;
    }
    
    bool operator!=(const InlinePoolAllocator&) const
    {
        return true;
    }
    
private:
    void* FallbackAlloc(size_t nBytes)
    {
        ++fallbackCount;

        auto& mmgr = MemoryManager::GetInstance();
        auto ptr = mmgr.SysAllocate(nBytes);

        return ptr;
    }

    void* AllocateBytes(size_t nBytes)
    {
        constexpr size_t unit = sizeof(T);
        constexpr size_t bufferSizeBytes = BufferSize * unit;

        if (nBytes > bufferSizeBytes)
            return FallbackAlloc(nBytes);

        if (unlikely(nBytes == 0))
            return &buffer[0][0];

        int index = indexHint;
        for (int i = 0; i < NumBuffers; ++i, ++index)
        {
            index = index >= NumBuffers ? 0 : index;
            if (isAllocated[index])
                continue;

            indexHint = index + 1;
            if (indexHint >= NumBuffers)
                indexHint = 0;

            isAllocated[index] = true;
            auto ptr = &buffer[index][0];

#ifdef __MEMOR_STATISTICS__
            auto& mmgr = MemoryManager::GetInstance();
            mmgr.ReportAllocation(id, ptr, nBytes, bufferSizeBytes);
#endif // __MEMOR_STATISTICS__

            return ptr;
        }

        return FallbackAlloc(nBytes);
    }

    void DeallocateBytes(void* ptr, size_t nBytes)
    {
        if (unlikely(ptr == nullptr || nBytes == 0))
            return;

        for (int i = 0; i < NumBuffers; ++i)
        {
            if (ptr != &buffer[i][0])
                continue;

#ifdef __MEMOR_STATISTICS__
            auto& mmgr = MemoryManager::GetInstance();
            constexpr size_t unit = sizeof(T);
            constexpr size_t bufferSizeBytes = BufferSize * unit;
            mmgr.ReportDeallocation(id, ptr, nBytes, bufferSizeBytes);
#endif // __MEMOR_STATISTICS__

            isAllocated[i] = false;
            indexHint = i;

            return;
        }

        auto& mmgr = MemoryManager::GetInstance();
        mmgr.SysDeallocate(ptr, nBytes);
    }

    void RegisterAllocator()
    {
        auto& mmgr = MemoryManager::GetInstance();
        
        auto allocFunc = [this](size_t nBytes) -> void*
        {
            return static_cast<void*>(AllocateBytes(nBytes));
        };
        
        auto deallocFunc = [this](void* ptr, size_t nBytes)
        {
            DeallocateBytes(ptr, nBytes);
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
#include "Test/TestCollection.h"

namespace HE
{

class InlinePoolAllocatorTest : public TestCollection
{
public:
    
    InlinePoolAllocatorTest()
        : TestCollection("InlinePoolAllocatorTest")
    {
    }
    
protected:
    virtual void Prepare() override;
};

} // HE
#endif //__UNIT_TEST__
