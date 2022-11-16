// Created by mooming.go@gmail.com, 2022

#include "MultiPoolAllocator.h"

#include "Config/BuildConfig.h"
#include "Log/Logger.h"
#include "Memory/MemoryManager.h"
#include "String/StringBuilder.h"
#include <algorithm>
#include <bit>
#include <memory>


namespace HE
{

MultiPoolAllocator::MultiPoolAllocator(const char* inName
    , size_t allocationUnit, size_t minBlockSize)

    : id(InvalidAllocatorID)
    , parentID(InvalidAllocatorID)
    , name(inName)
    , bankSize(allocationUnit)
    , minBlock(minBlockSize)
    , fallbackCount(0)
{
    using namespace HSTL;

    auto& mmgr = MemoryManager::GetInstance();
    parentID = mmgr.GetCurrentAllocatorID();

    AllocatorScope scope(parentID);

    auto allocFunc = [this](size_t n) -> void*
    {
        return Allocate(n);
    };

    auto deallocFunc = [this](void* ptr, size_t n)
    {
        Deallocate(ptr, n);
    };

    id = mmgr.Register(name, false, 0, allocFunc, deallocFunc);
}

MultiPoolAllocator::MultiPoolAllocator(const char* inName
    , TInitializerList initialConfigurations
    , size_t allocationUnit, size_t minBlockSize)

    : id(InvalidAllocatorID)
    , parentID(InvalidAllocatorID)
    , name(inName)
    , bankSize(allocationUnit)
    , minBlock(minBlockSize)
    , fallbackCount(0)
{
    using namespace HSTL;

    auto& mmgr = MemoryManager::GetInstance();
    parentID = mmgr.GetCurrentAllocatorID();

    AllocatorScope scope(parentID);

    HInlineVector<PoolConfig, 256> vlist;
    vlist.insert(vlist.end(), initialConfigurations);
    std::sort(vlist.begin(), vlist.end());
    
    banks.reserve(vlist.size());

    for (auto& config : vlist)
    {
        InlineStringBuilder<1024> str;
        str << name << '_' << config.blockSize
            << '_' << config.numberOfBlocks;
        
        banks.emplace_back(str.c_str()
            , config.blockSize, config.numberOfBlocks);
    }

    auto allocFunc = [this](size_t n) -> void*
    {
        return Allocate(n);
    };

    auto deallocFunc = [this](void* ptr, size_t n)
    {
        Deallocate(ptr, n);
    };

    id = mmgr.Register(name, false, 0, allocFunc, deallocFunc);
}

MultiPoolAllocator::~MultiPoolAllocator()
{
    PrintUsage();

    banks.clear();

    auto& mmgr = MemoryManager::GetInstance();
    mmgr.Deregister(GetID());
}

void* MultiPoolAllocator::Allocate(size_t requested)
{
    if (unlikely(requested <= 0))
        return nullptr;

    auto index = GetPoolIndex(requested);
    if (index >= banks.size())
    {
        ++fallbackCount;

        auto blockSize = CalculateBlockSize(requested);
        auto numBlocks = CalculateNumberOfBlocks(bankSize, blockSize);

        auto& bank = GenerateBank(blockSize, numBlocks);
        auto ptr = bank.Allocate(requested);

#ifdef PROFILE_ENABLED
        {
            auto& mmgr = MemoryManager::GetInstance();
            mmgr.ReportFallback(GetID(), ptr, requested);
        }
#endif // PROFILE_ENABLED

        return ptr;
    }
    
    auto& pool = banks[index];
    auto ptr = pool.Allocate(requested);
    
    return ptr;
}

void MultiPoolAllocator::Deallocate(void* ptr, size_t size)
{
    if (unlikely(ptr == nullptr))
    {
        Assert(size == 0);
        return;
    }

    auto index = GetPoolIndex(ptr);
    if (index >= banks.size())
    {
        auto log = Logger::Get(name);
        log.OutFatalError([ptr](auto& ls)
        {
            ls << ptr << " is allocated by another allocator.";
        });

        return;
    }
    
    auto& pool = banks[index];
    Assert(size <= pool.GetBlockSize());

    pool.Deallocate(ptr, size);
}

size_t MultiPoolAllocator::GetUsage() const
{
    size_t usage = 0;
    
    for (auto& pool : banks)
    {
        usage += pool.GetUsage();
    }
    
    return usage;
}

size_t MultiPoolAllocator::GetAvailableMemory() const
{
    size_t availableMemory = 0;
    
    for (auto& pool : banks)
    {
        availableMemory += pool.GetAvailableMemory();
    }
    
    return availableMemory;
}

size_t MultiPoolAllocator::GetCapacity() const
{
    size_t capacity = 0;
    
    for (auto& pool : banks)
    {
        capacity += pool.GetCapacity();
    }
    
    return capacity;
}

void MultiPoolAllocator::PrintUsage() const
{
    using namespace std;
    
    auto log = Logger::Get(name, ELogLevel::Info);
    log.Out([this](auto& ls)
    {
        ls << hendl << "## MultipoolAllocator(" << name
            <<  ") Usage ##" << hendl;
        
        for (auto& pool : banks)
        {
            ls << '[' << pool.GetBlockSize() << "] Available Memory = "
                << pool.GetAvailableMemory() << hendl;
        }
    });
}

size_t MultiPoolAllocator::GetPoolIndex(size_t nBytes) const
{
    size_t index = 0;
    
    for (auto& pool : banks)
    {
        if (pool.GetAvailableBlocks() <= 0)
            continue;

        if (nBytes <= pool.GetBlockSize())
        {
            return index;
        }
        
        ++index;
    }
    
    return index;
}

size_t MultiPoolAllocator::GetPoolIndex(void* ptr) const
{
    size_t index = 0;
    
    for (auto& pool : banks)
    {
        if (pool.IsMine(ptr))
        {
            return index;
        }
        
        ++index;
    }
    
    return index;
}

size_t MultiPoolAllocator::CalculateBlockSize(size_t requested) const
{
    size_t blockSize = (requested + minBlock - 1) / minBlock;
    blockSize = std::bit_ceil(blockSize);
    blockSize *= minBlock;
    blockSize = std::max(minBlock, blockSize);

    return blockSize;
}

size_t MultiPoolAllocator::CalculateNumberOfBlocks(size_t bankSize, size_t blockSize) const
{
    size_t numberOfBlocks = (bankSize + blockSize - 1) / blockSize;
    numberOfBlocks = std::max(MinNumberOfBlocks, numberOfBlocks);

    return numberOfBlocks;
}

PoolAllocator& MultiPoolAllocator::GenerateBank(size_t blockSize, size_t numberOfBlocks)
{
    AllocatorScope scope(parentID);

    Assert(blockSize > 0);
    Assert(numberOfBlocks > 0);

    InlineStringBuilder<1024> str;
    str << name << '_' << blockSize << '_' << numberOfBlocks;

    AllocatorScope allocScope(MemoryManager::SystemAllocatorID);
    auto& bank = banks.emplace_back(str.c_str(), blockSize, numberOfBlocks);

    auto log = Logger::Get(name);
    log.Out(ELogLevel::Verbose, [&str](auto& ls)
    {
        ls << "The bank[" << str.c_str() << "] has been generated. ";
    });

    return bank;
}

} // HE

#ifdef __UNIT_TEST__
#include "AllocatorScope.h"
#include "MemoryManager.h"


namespace HE
{

void MultiPoolAllocatorTest::Prepare()
{
    AddTest("Basic Construction", [this](auto& ls)
    {
        MultiPoolAllocator allocator("TestMultiPoolAlloc"
            , {{64, 1024}, {128, 1024}, {256, 1024}, {512, 1024}, {1024, 1024}, {2048, 1024}, {4096, 1024} });

        ls << "Capacity = " << allocator.GetCapacity() << lf;

        AllocatorScope scope(allocator);
    });

    AddTest("Allocation 0", [this](auto& ls)
    {
        MultiPoolAllocator allocator("TestMultiPoolAlloc"
            , {{64, 1024}, {128, 1024}, {256, 1024}, {512, 1024}, {1024, 1024}, {2048, 1024}, {4096, 1024} });

        ls << "Capacity = " << allocator.GetCapacity() << lf;

        AllocatorScope scope(allocator);
        
        auto& mmgr = MemoryManager::GetInstance();
        mmgr.AllocateBytes(0);
    });

    AddTest("Multiple Allocations & Fallback", [this](auto& ls)
    {
        MultiPoolAllocator allocator("TestMultiPoolAlloc"
            , {{64, 1024}, {128, 1024}, {256, 1024}, {512, 1024}, {1024, 1024}, {2048, 1024}, {4096, 1024} });

        ls << "Capacity = " << allocator.GetCapacity() << lf;

        AllocatorScope scope(allocator);
        
        auto& mmgr = MemoryManager::GetInstance();
        void* pointers[] = {
            mmgr.AllocateBytes(0),
            mmgr.AllocateBytes(8),
            mmgr.AllocateBytes(16),
            mmgr.AllocateBytes(32),
            mmgr.AllocateBytes(97),
            mmgr.AllocateBytes(110),
            mmgr.AllocateBytes(140),
            mmgr.AllocateBytes(270),
            mmgr.AllocateBytes(4032),
            mmgr.AllocateBytes(5000),
            mmgr.AllocateBytes(8000)
        };

        for (auto ptr : pointers)
        {
            mmgr.DeallocateBytes(ptr, 0);
        }

        if (allocator.GetFallbackCount() != 2)
        {
            ls << "Fallback count mismatched. FallbackCount = "
                << allocator.GetFallbackCount() << ", but 2 expected." << lferr;
        }
    });
}

} // HE
#endif //__UNIT_TEST__
