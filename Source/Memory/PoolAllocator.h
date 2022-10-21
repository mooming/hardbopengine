// Created by mooming.go@gmail.com, 2017

#pragma once

#include "AllocatorID.h"
#include "MemoryManager.h"
#include "OSAL/OSMemory.h"
#include "String/StaticString.h"
#include "System/CommonUtil.h"
#include "System/Debug.h"
#include "System/Types.h"
#include <algorithm>
#include <limits>


namespace HE
{

template <typename TIndex = unsigned short>
class PoolAllocator final
{
    static_assert(std::is_integral<TIndex>::value);
    static_assert(std::is_unsigned<TIndex>::value);

    using TSize = size_t;

private:
    TAllocatorID id;
    StaticString name;
    
    TIndex blockSize;
    TIndex numberOfBlocks;
    TIndex numberOfFreeBlocks;
    
    TSize maxUsage;
    TSize fallbackCount;
    
    Byte* buffer;
    Pointer availables;
    
public:
    PoolAllocator(const char* name, TSize inBlockSize, size_t numberOfBlocks)
        : id(InvalidAllocatorID)
        , name(name)
        , blockSize(OS::GetAligned(std::max(inBlockSize, sizeof(TIndex)), sizeof(TIndex)))
        , numberOfBlocks(numberOfBlocks)
        , numberOfFreeBlocks(numberOfBlocks)
        , maxUsage(0)
        , fallbackCount(0)
        , buffer(nullptr)
    {
        FatalAssert(blockSize > 0);
        FatalAssert(numberOfBlocks <= std::numeric_limits<TIndex>::max());

        auto& mmgr = MemoryManager::GetInstance();
        const TSize totalSize = blockSize * numberOfBlocks;

        buffer = static_cast<Byte*>(mmgr.Allocate(totalSize));
        availables = &buffer[0];

        for (TIndex i = 0; i < numberOfBlocks; ++i)
        {
            auto cursor = &buffer[i * blockSize];
            SetAs<TIndex>(cursor, i + 1);
        }

        auto allocFunc = [this](size_t n)
        {
            return Allocate(n);
        };

        auto deallocFunc = [this](void* ptr, size_t)
        {
            Deallocate(ptr);
        };

        id = mmgr.Register(name, false, totalSize, allocFunc, deallocFunc);
    }
    
    ~PoolAllocator()
    {
        FatalAssert(buffer != nullptr);
        FatalAssert(id != InvalidAllocatorID);

        auto& mmgr = MemoryManager::GetInstance();
        const size_t totalSize = blockSize * numberOfBlocks;
        mmgr.Deallocate(buffer, totalSize);
        mmgr.Deregister(GetID());

        buffer = nullptr;
        id = InvalidAllocatorID;
    }

    Pointer Allocate(size_t size)
    {
        const auto availble = blockSize - 1;
        if (unlikely(size > availble))
        {
            auto& mmgr = MemoryManager::GetInstance();
            mmgr.LogWarning([size, availble](auto& ls)
            {
                ls << "The requested size " << size
                    << " is exceeding its limit, " << availble << '.';
            });

            auto ptr = mmgr.SysAllocate(size);
            ++fallbackCount;

            return ptr;
        }

        return AllocateBlock();
    }
    
    void Deallocate(Pointer ptr)
    {
        if (unlikely(ptr == nullptr))
            return;

        if (unlikely(!IsMine(ptr)))
        {
            auto& mmgr = MemoryManager::GetInstance();
            mmgr.SysDeallocate(ptr, 0);
            return;
        }

        if (availables)
        {
            const auto index = GetIndex(availables);
            if (unlikely(index > numberOfBlocks))
            {
                auto& mmgr = MemoryManager::GetInstance();
                mmgr.LogError([ptr](auto& logStream)
                {
                    logStream << ptr << " is not alloacted by this.";
                });

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
    }

    bool IsMine(Pointer ptr) const
    {
        auto bytePtr = reinterpret_cast<Byte*>(ptr);
        auto offset = static_cast<size_t>(bytePtr - buffer);
        auto totalSize = blockSize * numberOfBlocks;

        return buffer <= bytePtr && offset < totalSize;
    }

    inline auto GetID() const { return id; }
    inline auto GetName() const { return name; }
    inline size_t GetSize(const Pointer) const { return blockSize; }
    inline auto GetFallbackCount() const { return fallbackCount; }
    
    inline size_t GetUsage() const { return (numberOfBlocks - numberOfFreeBlocks) * blockSize; }
    inline size_t GetMaxUsage() const { return maxUsage; }
    inline size_t GetAvailableMemory() const { return numberOfFreeBlocks * blockSize; }
    inline size_t GetCapacity() const { return numberOfBlocks * blockSize; }
    inline auto GetBlockSize() const { return blockSize; }
    inline auto NumberOfFreeBlocks() const { return numberOfFreeBlocks; }
    
private:
    TIndex GetIndex(Pointer ptr) const
    {
        auto bytePtr = reinterpret_cast<Byte*>(ptr);
        auto delta = bytePtr - buffer;
        TIndex index = delta / blockSize;

        return index;
    }

    TIndex ReadNextIndex(Pointer ptr) const
    {
        TIndex index = GetAs<TIndex>(ptr);
        Assert(index <= numberOfBlocks
            , "PoolAllocator: out of bounds index = %u / %u", index, numberOfBlocks);

        return index;
    }
    
    void WriteNextIndex(Pointer ptr, TIndex index)
    {
        Assert(index < numberOfBlocks
            , "PoolAllocator: out of bounds index. The index "
               , index, " should be less than ", numberOfBlocks);

        SetAs<TIndex>(ptr, index);
    }

    Pointer AllocateBlock()
    {
        if (!availables)
        {
            auto& mmgr = MemoryManager::GetInstance();
            mmgr.LogError([this](auto& ls)
            {
                ls << "No avaiable memory blocks. Usage = "
                    << GetUsage() << " / " << GetAvailableMemory();
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

        --numberOfFreeBlocks;
        maxUsage = std::max(maxUsage, GetUsage());

        return ptr;
    }
};
} // HE

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"


namespace HE
{
class PoolAllocatorTest : public TestCollection
{
public:
    PoolAllocatorTest() : TestCollection("PoolAllocatorTest")
    {
    }
    
protected:
    virtual void Prepare() override;
};
} // HE
#endif //__UNIT_TEST__
