// Created by mooming.go@gmail.com, 2017

#include "PoolAllocator.h"

#include "MemoryManager.h"
#include "System/Debug.h"
#include "System/Exception.h"
#include "System/CommonUtil.h"


using namespace HE;

PoolAllocator::PoolAllocator(Index blockSize, Index numberOfBlocks)
    : id(InvalidAllocatorID)
    , blockSize(blockSize)
    , numberOfBlocks(numberOfBlocks)
    , numberOfFreeBlocks(numberOfBlocks)
    , buffer(nullptr)
    , indexType(IndexType::U32)
{
    if (blockSize == 0)
    {
        throw Exception(__FILE__, __LINE__, "Pool size is zero\n");
    }
    else if (blockSize == 1)
    {
        if (numberOfBlocks > 0xFF)
        {
            throw Exception(__FILE__, __LINE__
                , "Number of blocks should be less than %u, when block size is %u.\n"
                , 0xFF, blockSize);
        }
    }
    else if (blockSize <= 3)
    {
        if (numberOfBlocks > 0xFFFF)
        {
            throw Exception(__FILE__, __LINE__
                , "Number of blocks should be less than %u, when block size is %u.\n"
                , 0xFFFF, blockSize);
        }
    }

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
}

PoolAllocator::~PoolAllocator()
{
    auto& mmgr = MemoryManager::GetInstance();
    const size_t totalSize = blockSize * numberOfBlocks;
    mmgr.Deallocate(buffer, totalSize);
}

Pointer PoolAllocator::Allocate()
{
    if (availables)
    {
        void* ptr = availables;
        size_t index = GetIndex(ptr);
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
    return nullptr;
}

Pointer PoolAllocator::Allocate(size_t size)
{
    if (size <= blockSize)
    {
        return Allocate();
    }
    else
    {
        throw Exception(__FILE__, __LINE__
            , "Pool allocation failed. required = %u, size of block = %u"
            , size, blockSize);
    }
}
void PoolAllocator::Deallocate(const Pointer ptr)
{
    if (ptr)
    {
        if (availables)
        {
            const uint32_t index = 0xFFFFFFFF
                & ((reinterpret_cast<Byte*>(availables) - buffer) / blockSize);

            if (index > numberOfBlocks)
            {
                throw Exception(__FILE__, __LINE__
                    , "PoolAllocator: Deallocation failed. Invalid Index = %u\n", index);
            }

            SetIndex(ptr, index);
        }
        else
        {
            SetIndex(ptr, numberOfBlocks);
        }

        availables = ptr;
        ++numberOfFreeBlocks;
    }
}

Index PoolAllocator::GetIndex(Pointer ptr)
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


void PoolAllocator::SetIndex(Pointer ptr, Index index)
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

#ifdef __UNIT_TEST__
#include <iostream>

bool PoolAllocatorTest::DoTest()
{
    for (int i = 1; i < 100; ++i)
    {
        PoolAllocator pool(i, 100);
    }

    {
        PoolAllocator pool(100, 100);
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
