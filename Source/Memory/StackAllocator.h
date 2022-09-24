// Created by mooming.go@gmail.com, 2017

#pragma once

#include "AllocatorID.h"
#include "System/Types.h"


namespace HE
{
class StackAllocator final
{
public:
    using This = StackAllocator;
    using SizeType = size_t;
    
private:
    TAllocatorID id;
    SizeType capacity;
    SizeType freeSize;
    SizeType cursor;
    
    union
    {
        Byte* buffer;
        Pointer bufferPtr;
    };
    
public:
    StackAllocator(const char* name, size_t capacity);
    ~StackAllocator();
    
    Pointer Allocate(size_t size);
    void Deallocate(const Pointer ptr);
    
    size_t Usage() const;
    size_t Available() const;
    
    inline auto GetID() const { return id; }
    inline size_t GetSize(const Pointer) const { return 0; }
    
private:
    bool IsMine(Pointer ptr) const;
};
} // HE

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace HE
{

class StackAllocatorTest : public TestCollection
{
public:
    StackAllocatorTest() : TestCollection("StackAllocatorTest")
    {
    }
    
protected:
    virtual void Prepare() override;
};

} // HE
#endif //__UNIT_TEST__
