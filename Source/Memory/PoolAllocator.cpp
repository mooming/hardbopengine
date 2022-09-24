// Created by mooming.go@gmail.com, 2017

#include "PoolAllocator.h"

#include "MemoryManager.h"
#include "System/Debug.h"
#include "System/Exception.h"
#include "System/CommonUtil.h"


using namespace HE;

PoolAllocator::PoolAllocator(const char* name,Index blockSize, Index numberOfBlocks)
    : id(InvalidAllocatorID)
    , name(name)
    , blockSize(blockSize)
    , numberOfBlocks(numberOfBlocks)
    , numberOfFreeBlocks(numberOfBlocks)
    , maxUsage(0)
    , fallbackCount(0)
    , buffer(nullptr)
    , indexType(IndexType::U32)
{
    FatalAssert(blockSize > 0);
    FatalAssert(blockSize != 1 || numberOfBlocks <= 0xFF
        , "Number of blocks should be less than "
        , 0xFF, ", when block size is ", blockSize);

    FatalAssert(blockSize > 3 || numberOfBlocks <= 0xFFFF
        , "Number of blocks should be less than "
        , 0xFFFF, ", when block size is ", blockSize);

    if (numberOfBlocks <= 0xFF)
    {
        indexType = IndexType::U8;
    }
    else if (numberOfBlocks <= 0xFFFF)
    {
        indexType = IndexType::U16;
    }

    auto& mmgr = MemoryManager::GetInstance();
    const size_t totalSize = blockSize * numberOfBlocks;
    buffer = static_cast<Byte*>(mmgr.Allocate(totalSize));

    availables = &buffer[0];

    switch (indexType)
    {
    case IndexType::U8:
        for (uint32_t i = 0; i < numberOfBlocks; ++i)
        {
            Pointer tmp = &buffer[i * blockSize];
            SetAs<uint8_t>(tmp, static_cast<uint8_t>(i + 1));
        }
        break;

    case IndexType::U16:
        for (uint32_t i = 0; i < numberOfBlocks; ++i)
        {
            Pointer tmp = &buffer[i * blockSize];
            SetAs<uint16_t>(tmp, static_cast<uint16_t>(i + 1));
        }
        break;

    default:
        for (uint32_t i = 0; i < numberOfBlocks; ++i)
        {
            Pointer tmp = &buffer[i * blockSize];
            SetAs<uint32_t>(tmp, static_cast<uint32_t>(i + 1));
        }
        break;
    }
    
    auto allocFunc = [this](size_t n) -> void*
    {
        return Allocate(n);
    };
    
    auto deallocFunc = [this](void* ptr, size_t)
    {
        Deallocate(ptr);
    };
    
    id = mmgr.Register(name, false, totalSize, allocFunc, deallocFunc);
}

PoolAllocator::~PoolAllocator()
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

Pointer PoolAllocator::Allocate(size_t size)
{
    if (unlikely(size > blockSize))
    {
        auto& mmgr = MemoryManager::GetInstance();
        mmgr.LogError([size, blockSize = blockSize](auto& logStream)
        {
            logStream << "The requested size " << size
                << " is exceeding its limit, " << blockSize << '.';
        });

        auto ptr = mmgr.SysAllocate(size);
        ++fallbackCount;
        
        return ptr;
    }
    
    return Allocate();
}

void PoolAllocator::Deallocate(Pointer ptr)
{
    if (ptr == nullptr)
        return;
    
    if (unlikely(!IsValid(ptr)))
    {
        auto& mmgr = MemoryManager::GetInstance();
        mmgr.SysDeallocate(ptr, 0);
        return;
    }

    if (availables)
    {
        const auto index =GetIndex(availables);
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

bool PoolAllocator::IsValid(Pointer ptr) const
{
    auto bytePtr = reinterpret_cast<Byte*>(ptr);
    auto offset = static_cast<size_t>(bytePtr - buffer);
    auto totalSize = blockSize * numberOfBlocks;
    
    return buffer <= bytePtr && offset < totalSize;
}

Index PoolAllocator::GetIndex(Pointer ptr) const
{
    auto bytePtr = reinterpret_cast<Byte*>(ptr);
    auto delta = bytePtr - buffer;
    auto index = delta / blockSize;

    return index;
}

Index PoolAllocator::ReadNextIndex(Pointer ptr) const
{
    uint32_t index = 0;

    switch (indexType)
    {
    case IndexType::U8:
        index = GetAs<uint8_t>(ptr);
        break;

    case IndexType::U16:
        index = GetAs<uint16_t>(ptr);
        break;

    default:
        index = GetAs<uint32_t>(ptr);
        break;
    }

    Assert(index <= numberOfBlocks
        , "PoolAllocator: out of bounds index = %u / %u", index, numberOfBlocks);

    return index;
}


void PoolAllocator::WriteNextIndex(Pointer ptr, Index index)
{
    Assert(index < numberOfBlocks
        , "PoolAllocator: out of bounds index = %u / %u", index, numberOfBlocks);

    switch (indexType)
    {
    case IndexType::U8:
        SetAs<uint8_t>(ptr, static_cast<uint8_t>(index));
        break;

    case IndexType::U16:
        SetAs<uint16_t>(ptr, static_cast<uint16_t>(index));
        break;

    default:
        SetAs<uint32_t>(ptr, static_cast<uint32_t>(index));
        break;
    }
}

Pointer PoolAllocator::Allocate()
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

#ifdef __UNIT_TEST__

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
        PoolAllocator pool("TestPoolAllocator",100, 100);

        for (int i = 0; i < 100; ++i)
        {
            auto ptr = pool.Allocate(50);
            auto size = pool.GetSize(ptr);
            if (size != 100)
            {
                ls << "The size is incorrect. " << size
                    << ", but 100 expected." << lferr;
                break;
            }

            pool.Deallocate(ptr);
        }
    });
}

#endif //__UNIT_TEST__
