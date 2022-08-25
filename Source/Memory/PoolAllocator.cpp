// Created by mooming.go@gmail.com, 2017

#include "PoolAllocator.h"

#include "MemoryManager.h"
#include "Log/Logger.h"
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
    auto& mmgr = MemoryManager::GetInstance();
    const size_t totalSize = blockSize * numberOfBlocks;
    mmgr.Deallocate(buffer, totalSize);
}

Pointer PoolAllocator::Allocate(size_t size)
{
    if (unlikely(size > blockSize))
    {
        auto errorLog = Logger::Get(GetName(), ELogLevel::Error);
        errorLog.Out([size, blockSize = blockSize](auto& logStream)
        {
            logStream << "The requested size " << size
                << " is exceeding its limit, " << blockSize << '.';
        });

        return nullptr;
    }
    
    return Allocate();
}
void PoolAllocator::Deallocate(Pointer ptr)
{
    if (ptr == nullptr)
        return;

    if (availables)
    {
        const auto index =GetIndex(availables);
        if (unlikely(index > numberOfBlocks))
        {
            auto errorLog = Logger::Get(GetName(), ELogLevel::Error);
            errorLog.Out([ptr](auto& logStream)
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
        return nullptr;

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

    return ptr;
}

#ifdef __UNIT_TEST__
#include <iostream>

bool PoolAllocatorTest::DoTest()
{
    for (int i = 1; i < 100; ++i)
    {
        PoolAllocator pool("TestPoolAllocator", i, 100);
    }

    {
        PoolAllocator pool("TestPoolAllocator",100, 100);
        for (int i = 0; i < 100; ++i)
        {
            auto ptr = pool.Allocate(50);
            auto size = pool.GetSize(ptr);
            if (size != 100)
                return false;

            pool.Deallocate(ptr);
        }
    }

    return true;
}
#endif //__UNIT_TEST__
