// Created by mooming.go@gmail.com, 2017

#include "PoolAllocator.h"

#include "MemoryManager.h"
#include "OSAL/OSMemory.h"
#include "System/CommonUtil.h"
#include "System/Debug.h"
#include <algorithm>
#include <limits>


namespace HE
{
#ifdef PROFILE_ENABLED
PoolAllocator::PoolAllocator(const char* name, TSize inBlockSize, TSize numberOfBlocks
    , const std::source_location location)
#else // PROFILE_ENABLED
PoolAllocator::PoolAllocator(const char* name, TSize inBlockSize, TIndex numberOfBlocks)
#endif // PROFILE_ENABLED
    : id(InvalidAllocatorID)
    , name(name)
    , blockSize(OS::GetAligned(std::max(inBlockSize, sizeof(TSize)), sizeof(TSize)))
    , numberOfBlocks(numberOfBlocks)
    , numberOfFreeBlocks(numberOfBlocks)
    , buffer(nullptr)
#ifdef PROFILE_ENABLED
    , maxUsedBlocks(0)
    , srcLocation(location)
#endif // PROFILE_ENABLED
{
    Assert(blockSize >= sizeof(TSize));

    auto& mmgr = MemoryManager::GetInstance();
    const TSize totalSize = blockSize * numberOfBlocks;

    buffer = static_cast<Byte*>(mmgr.AllocateBytes(totalSize));
    availables = &buffer[0];

    for (TSize i = 0; i < numberOfBlocks; ++i)
    {
        auto cursor = &buffer[i * blockSize];
        SetAs<TSize>(cursor, i + 1);
    }

    auto allocFunc = [this](size_t n)
    {
        return Allocate(n);
    };

    auto deallocFunc = [this](void* ptr, size_t n)
    {
        Deallocate(ptr, n);
    };

    id = mmgr.Register(name, false, totalSize, allocFunc, deallocFunc);
}

PoolAllocator::PoolAllocator(PoolAllocator&& rhs) noexcept
    : id(rhs.id)
    , name(rhs.name)
    , blockSize(rhs.blockSize)
    , numberOfBlocks(rhs.numberOfBlocks)
    , numberOfFreeBlocks(rhs.numberOfFreeBlocks)
    , buffer(rhs.buffer)
    , availables(rhs.availables)
#ifdef PROFILE_ENABLED
    , maxUsedBlocks(rhs.maxUsedBlocks)
    , srcLocation(std::move(rhs.srcLocation))
#endif // PROFILE_ENABLED
{
    rhs.id = InvalidAllocatorID;
    rhs.name = StaticString();
    rhs.blockSize = 0;
    rhs.numberOfBlocks = 0;
    rhs.numberOfFreeBlocks = 0;
    rhs.buffer = nullptr;
    rhs.availables = nullptr;

#ifdef PROFILE_ENABLED
    rhs.maxUsedBlocks = 0;
#endif // PROFILE_ENABLED

    auto& mmgr = MemoryManager::GetInstance();
    const TSize totalSize = blockSize * numberOfBlocks;

    auto allocFunc = [this](size_t n)
    {
        return Allocate(n);
    };

    auto deallocFunc = [this](void* ptr, size_t n)
    {
        Deallocate(ptr, n);
    };

    mmgr.Update(GetID(), [totalSize, allocFunc, deallocFunc](auto& proxy)
    {
#ifdef PROFILE_ENABLED
        auto& stats = proxy.stats;
        stats.capacity = totalSize;
#endif // PROFILE_ENABLED

        proxy.allocate = allocFunc;
        proxy.deallocate = deallocFunc;
    }, "Move Constructor");
}

PoolAllocator::~PoolAllocator()
{
    if (id == InvalidAllocatorID)
    {
        Assert(buffer == nullptr);
        return;
    }

    Assert(id != InvalidAllocatorID);
    Assert(buffer != nullptr);

    auto& mmgr = MemoryManager::GetInstance();
    const size_t totalSize = blockSize * numberOfBlocks;

    mmgr.DeallocateBytes(buffer, totalSize);

#ifdef PROFILE_ENABLED
    mmgr.Deregister(GetID(), srcLocation);
#else // PROFILE_ENABLED
    mmgr.Deregister(GetID());
#endif // PROFILE_ENABLED

    buffer = nullptr;
    id = InvalidAllocatorID;
}

PoolAllocator& PoolAllocator::operator= (PoolAllocator&& rhs) noexcept
{
    this->~PoolAllocator();
    new (this) PoolAllocator(std::move(rhs));

    return *this;
}

bool PoolAllocator::operator< (const PoolAllocator& rhs) const noexcept
{
    if (blockSize < rhs.blockSize)
        return true;

    if (blockSize == rhs.blockSize)
    {
        if (GetAvailableBlocks() > rhs.GetAvailableBlocks())
            return true;
    }

    return false;
}

Pointer PoolAllocator::Allocate(size_t size)
{
    if (unlikely(size > blockSize))
    {
        auto& mmgr = MemoryManager::GetInstance();
        mmgr.LogWarning([this, size](auto& ls)
            {
                ls << "The requested size " << size
                    << " is exceeding its limit, " << blockSize << '.';
            });

        auto ptr = mmgr.SysAllocate(size);

#ifdef PROFILE_ENABLED
        mmgr.ReportFallback(id, ptr, size);
#endif // PROFILE_ENABLED

        return ptr;
    }

    auto ptr = AllocateBlock();
#ifdef PROFILE_ENABLED
    {
        Assert(size <= blockSize);
        auto& mmgr = MemoryManager::GetInstance();
        mmgr.ReportAllocation(id, ptr, size, blockSize);
    }
#endif // PROFILE_ENABLED

    return ptr;
}

void PoolAllocator::Deallocate(Pointer ptr, size_t size)
{
    if (unlikely(ptr == nullptr))
        return;

    if (unlikely(!IsMine(ptr)))
    {
        auto& mmgr = MemoryManager::GetInstance();
        mmgr.SysDeallocate(ptr, 0);
        return;
    }

#ifdef PROFILE_ENABLED
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
        WriteNextIndex(ptr, numberOfBlocks - 1);
    }

    availables = ptr;

    ++numberOfFreeBlocks;

    Assert(numberOfFreeBlocks <= numberOfBlocks);
}

bool PoolAllocator::IsMine(Pointer ptr) const
{
    auto bytePtr = reinterpret_cast<Byte*>(ptr);
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
    Assert(index <= numberOfBlocks
        , "PoolAllocator: out of bounds index = %u / %u", index, numberOfBlocks);

    return index;
}

void PoolAllocator::WriteNextIndex(Pointer ptr, TSize index)
{
    Assert(index < numberOfBlocks
        , "PoolAllocator: out of bounds index. The index "
        , index, " should be less than ", numberOfBlocks);

    SetAs<TSize>(ptr, index);
}

Pointer PoolAllocator::AllocateBlock()
{
    if (!availables)
    {
        auto& mmgr = MemoryManager::GetInstance();
        mmgr.LogError([this](auto& ls)
            {
                ls << "No available memory blocks. Usage = "
                    << (numberOfBlocks - numberOfFreeBlocks) << " / "
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

#ifdef PROFILE_ENABLED
    maxUsedBlocks = std::max(maxUsedBlocks, numberOfBlocks - numberOfFreeBlocks);
#endif // PROFILE_ENABLED 

    return ptr;
}

} // HE

#ifdef __UNIT_TEST__

namespace HE
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
                ls << "The size is incorrect. " << size
                    << ", but 100 expected." << lferr;
                break;
            }

            pool.Deallocate(ptr, allocSize);
        }
    });
}

} // HE

#endif //__UNIT_TEST__
