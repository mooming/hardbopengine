// Created by mooming.go@gmail.com, 2022

#pragma once

#include "PoolAllocator.h"
#include "HSTL/HVector.h"
#include "String/StaticString.h"


namespace HE
{
class MultiPoolAllocator final
{
public:
    using This = MultiPoolAllocator;

    static constexpr size_t DefaultMinBlock = 16;
    static constexpr size_t DefaultBankUnit = 1024ULL * 1024;
    static constexpr size_t MinNumberOfBlocks = 16;

    struct PoolConfig final
    {
        size_t blockSize = 0;
        size_t numberOfBlocks = 0;

        inline bool operator < (const PoolConfig& rhs) const
        {
            return blockSize < rhs.blockSize;
        }
    };
    
private:
    using TInitializerList = std::initializer_list<PoolConfig>;
    
    TAllocatorID id;
    TAllocatorID parentID;
    StaticString name;
    HSTL::HVector<PoolAllocator> banks;
    size_t bankSize;
    size_t minBlock;
    size_t fallbackCount;
    
public:
    MultiPoolAllocator(const char* name
        , size_t allocationUnit = DefaultBankUnit, size_t minBlockSize = DefaultMinBlock);
    MultiPoolAllocator(const char* name, TInitializerList initialConfigurations
        , size_t allocationUnit = DefaultBankUnit, size_t minBlockSize = DefaultMinBlock);
    ~MultiPoolAllocator();
    
    void* Allocate(size_t size);
    void Deallocate(void* ptr, size_t size);
    
    inline auto GetName() const { return name; }
    inline auto GetID() const { return id; }
    inline auto GetFallbackCount() const { return fallbackCount; }
    
    size_t GetUsage() const;
    size_t GetAvailableMemory() const;
    size_t GetCapacity() const;
    
    void PrintUsage() const;
    
private:
    size_t GetBankIndex(size_t nBytes) const;
    size_t GetBankIndex(void* ptr) const;
    size_t CalculateBlockSize(size_t requested) const;
    size_t CalculateNumberOfBlocks(size_t bankSize, size_t blockSize) const;
    void GenerateBank(size_t blockSize, size_t numberOfBlocks);
};
} // HE

#ifdef __UNIT_TEST__

#include "Test/TestCollection.h"


namespace HE
{
class MultiPoolAllocatorTest : public TestCollection
{
public:
    MultiPoolAllocatorTest() : TestCollection("MultiPoolAllocatorTest")
    {
    }
    
protected:
    virtual void Prepare() override;
};
} // HE
#endif //__UNIT_TEST__
