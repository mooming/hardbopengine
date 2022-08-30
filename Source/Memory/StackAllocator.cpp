// Created by mooming.go@gmail.com, 2017

#include "StackAllocator.h"

#include "AllocatorScope.h"
#include "MemoryManager.h"
#include "System/Debug.h"
#include "System/Exception.h"
#include "System/CommonUtil.h"


using namespace HE;

StackAllocator::StackAllocator(const char* name, size_t capacity)
    : id(InvalidAllocatorID)
    , capacity(capacity)
    , freeSize(capacity)
    , cursor(0)
    , buffer(nullptr)
{
    FatalAssert(capacity > (sizeof(bool) + sizeof(SizeType))
        , "StackAllocator: Capacity is too small. Should be larger than %lu\n"
        , sizeof(bool) + sizeof(SizeType));
    
    auto& mmgr = MemoryManager::GetInstance();
    bufferPtr = mmgr.Allocate(capacity);
    
    auto allocFunc = [this](size_t n) -> void*
    {
        return Allocate(n);
    };
    
    auto deallocFunc = [this](void* ptr, size_t)
    {
        Deallocate(ptr);
    };
    
    id = mmgr.Register(name, false, capacity, allocFunc, deallocFunc);
}

StackAllocator::~StackAllocator()
{
    auto& mmgr = MemoryManager::GetInstance();
    mmgr.Deallocate(bufferPtr, capacity);
    mmgr.Deregister(GetID());
}

void *StackAllocator::Allocate (size_t size)
{
    // Structure
    // flag - 1 byte
    // bufer[N] - n byte
    // previousIndex - sizeType byte
    
    constexpr SizeType metaDataSize = sizeof(bool) + sizeof(SizeType);
    const size_t requiredSize = metaDataSize + size;
    
    if (freeSize < requiredSize)
    {
        using namespace std;
        auto& mmgr = MemoryManager::GetInstance();
        mmgr.LogError([this, size](auto& lout){
            lout << "StackAllocator: Not Enough Memory" << endl
                << "require = " << size << " : free = "
                << freeSize << " / " << capacity;
        });

        return nullptr;
    }
    
    SizeType previous = cursor;
    
    SetAs<bool>(&buffer[cursor], true);
    cursor += sizeof(bool);
    Pointer ptr = &buffer[cursor];
    cursor += size;
    
    SetAs<SizeType>(&buffer[cursor], static_cast<SizeType>(previous));
    
    cursor += sizeof(SizeType);
    freeSize -= requiredSize;
    
    return ptr;
}

void StackAllocator::Deallocate(const Pointer ptr)
{
    Assert(IsMine(ptr));
    
    Byte* bytePtr = reinterpret_cast<Byte*>(ptr) - 1;
    Assert(GetAs<bool>(bytePtr), "StackAllocator: Double Free Error\n");
    
    auto DoDealloc = [this, &bytePtr]() -> bool
    {
        Assert(cursor <= capacity, "Invalid cursor = %lu, capacity = %lu\n", cursor, capacity);
        Assert(cursor >= (sizeof(SizeType) + sizeof(bool)), "bad cursor = %lu, size type = %lu\n"
                      , cursor, sizeof(SizeType));
        
        Pointer sizePtr = &buffer[cursor - sizeof(SizeType)];
        size_t previous = GetAs<SizeType>(sizePtr);
        Assert(cursor > previous, "Invalid Cursor. Expecting cursor = %lu > previpus = %lu\n", cursor, previous);
        
        if (bytePtr == &buffer[previous])
        {
            const auto allocSize = cursor - previous;
            
            freeSize += allocSize;
            cursor = previous;
            
            if (cursor == 0)
            {
                return false;
            }
            else
            {
                bytePtr = &buffer[previous];
                return !GetAs<bool>(bytePtr);
            }
        }
        else
        {
            Assert(GetAs<bool>(bytePtr), "try to free %p twice.", bytePtr + 1);
            SetAs<bool>(bytePtr, false);
        }
        
        return false;
    };
    
    while (DoDealloc());
}

size_t StackAllocator::Available() const
{
    return freeSize;
}

size_t StackAllocator::Usage() const
{
    return capacity - freeSize;
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

#include <iostream>

#include "Container/Vector.h"
#include "String/String.h"


bool StackAllocatorTest::DoTest()
{
    using namespace std;
    
    StackAllocator stack("Test::StackAllocator", 1024 * 1024);
    AllocatorScope scope(stack.GetID());
    
    {
        Vector<int> a;
        a.push_back(0);
    }
    
    if (stack.Usage() != 0)
    {
        cerr << "Deallocation Failed. Usage should be zero, but " << stack.Usage() << endl;
        return false;
    }
    
    {
        Vector<int> a;
        a.push_back(0);
        Vector<int> b;
        b.push_back(1);
    }
    
    if (stack.Usage() != 0)
    {
        cerr << "Deallocation Failed. Usage should be zero, but " << stack.Usage() << endl;
        return false;
    }
    
    {
        String a = "0";
    }
    
    if (stack.Usage() != 0)
    {
        cerr << "Deallocation Failed. Usage should be zero, but " << stack.Usage() << endl;
        return false;
    }
    
    {
        String a = "0";
        String b = "1";
    }
    
    if (stack.Usage() != 0)
    {
        cerr << "Deallocation Failed. Usage should be zero, but " << stack.Usage() << endl;
        return false;
    }
    
    return true;
}

#endif //__UNIT_TEST__
