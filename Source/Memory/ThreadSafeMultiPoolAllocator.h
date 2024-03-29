// Created by mooming.go@gmail.com, 2022

#pragma once

#include "PoolAllocator.h"
#include "PoolConfig.h"
#include "Config/BuildConfig.h"
#include "HSTL/HVector.h"
#include "String/StaticString.h"
#include <mutex>


namespace HE
{

class ThreadSafeMultiPoolAllocator final
{
public:
    using This = ThreadSafeMultiPoolAllocator;

    static constexpr size_t DefaultMinBlock = 16;
    static constexpr size_t DefaultBankUnit = 1024ULL * 1024;
    static constexpr size_t MinNumberOfBlocks = 16;

private:
    using TInitializerList = std::initializer_list<PoolConfig>;

    TAllocatorID id;
    TAllocatorID parentID;
    StaticString name;

    std::mutex lock;
    HSTL::HVector<PoolAllocator> banks;
    size_t bankSize;
    size_t minBlock;

public:
    ThreadSafeMultiPoolAllocator(const char* name
        , size_t allocationUnit = DefaultBankUnit, size_t minBlockSize = DefaultMinBlock);
    ThreadSafeMultiPoolAllocator(const char* name, TInitializerList initialConfigurations
        , size_t allocationUnit = DefaultBankUnit, size_t minBlockSize = DefaultMinBlock);
    ~ThreadSafeMultiPoolAllocator();

    void* Allocate(size_t size);
    void Deallocate(void* ptr, size_t size);

    inline auto GetName() const { return name; }
    inline auto GetID() const { return id; }

    void PrintUsage();

#ifdef PROFILE_ENABLED
    void ReportConfiguration();
#endif // PROFILE_ENABLED

private:
    bool GenerateBanksByCache(class MemoryManager& mmgr);
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
class ThreadSafeMultiPoolAllocatorTest
    : public TestCollection
{
public:
    ThreadSafeMultiPoolAllocatorTest()
        : TestCollection("ThreadSafeMultiPoolAllocatorTest")
    {
    }

protected:
    virtual void Prepare() override;
};
} // HE
#endif //__UNIT_TEST__
