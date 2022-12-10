// Created by mooming.go@gmail.com, 2022

#pragma once

#include "AllocatorID.h"
#include <cstddef>
#include <cstdint>


namespace HE
{
class MonotonicAllocator final
{
public:
    using TSize = size_t;
    using TPointer = void*;

private:
    TAllocatorID id;

    TSize cursor;
    TSize capacity;
    TSize fallbackCount;

    union
    {
        uint8_t* buffer;
        TPointer bufferPtr;
    };

public:
    MonotonicAllocator(const char* name, TSize capacity);
    ~MonotonicAllocator();

    TPointer Allocate(size_t size);
    void Deallocate(const TPointer ptr, TSize size);

    size_t GetAvailable() const;
    size_t GetUsage() const;

    inline auto GetID() const { return id; }
};
} // HE

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace HE
{

class MonotonicAllocatorTest : public TestCollection
{
public:
    MonotonicAllocatorTest() : TestCollection("MonotonicAllocatorTest")
    {
    }

protected:
    virtual void Prepare() override;
};

} // HE
#endif //__UNIT_TEST__
