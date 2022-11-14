// Created by mooming.go@gmail.com, 2022

#include "MultiPoolAllocator.h"

#include "Log/Logger.h"
#include "Memory/MemoryManager.h"
#include "String/StringBuilder.h"
#include <algorithm>
#include <memory>


namespace HE
{

MultiPoolAllocator::MultiPoolAllocator(const char* inName, TInitializerList list)
    : id(InvalidAllocatorID)
    , name(inName)
    , fallbackCount(0)
{
    using namespace std;;
    using namespace HSTL;
    
    HInlineVector<PoolConfig, 256> vlist;
    vlist.insert(vlist.end(), list);
    std::sort(vlist.begin(), vlist.end());
    
    multiPool.reserve(vlist.size());

    for (auto& config : vlist)
    {
        InlineStringBuilder<1024> str;
        str << name << '_' <<  static_cast<int>(config.blockSize)
            << '_' << static_cast<int>(config.numberOfBlocks);
        
        multiPool.emplace_back(str.c_str()
            , config.blockSize, config.numberOfBlocks);
    }
    
    auto& mmgr = MemoryManager::GetInstance();
    
    auto allocFunc = [this](size_t n) -> void*
    {
        return Allocate(n);
    };
    
    auto deallocFunc = [this](void* ptr, size_t)
    {
        Deallocate(ptr);
    };
    
    id = mmgr.Register(name, false, 0, allocFunc, deallocFunc);
}

MultiPoolAllocator::~MultiPoolAllocator()
{
    PrintUsage();

    multiPool.clear();

    auto& mmgr = MemoryManager::GetInstance();
    mmgr.Deregister(GetID());
}

void* MultiPoolAllocator::Allocate(size_t size)
{
    auto index = GetPoolIndex(size);
    if (index >= multiPool.size())
    {
        auto& mmgr = MemoryManager::GetInstance();
        auto ptr = mmgr.SysAllocate(size);
        ++fallbackCount;
        
        return ptr;
    }
    
    auto& pool = multiPool[index];
    auto ptr = pool.Allocate(size);
    
    return ptr;
}

void MultiPoolAllocator::Deallocate(void* ptr)
{
    auto index = GetPoolIndex(ptr);
    if (index >= multiPool.size())
    {
        auto& mmgr = MemoryManager::GetInstance();
        mmgr.SysDeallocate(ptr, 0);
        
        return;
    }
    
    auto& pool = multiPool[index];
    pool.Deallocate(ptr);
}

size_t MultiPoolAllocator::GetUsage() const
{
    size_t usage = 0;
    
    for (auto& pool : multiPool)
    {
        usage += pool.GetUsage();
    }
    
    return usage;
}

size_t MultiPoolAllocator::GetAvailableMemory() const
{
    size_t availableMemory = 0;
    
    for (auto& pool : multiPool)
    {
        availableMemory += pool.GetAvailableMemory();
    }
    
    return availableMemory;
}

size_t MultiPoolAllocator::GetCapacity() const
{
    size_t capacity = 0;
    
    for (auto& pool : multiPool)
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
        
        for (auto& pool : multiPool)
        {
            ls << '[' << pool.GetBlockSize() << "] Max Usage = "
                << pool.GetMaxUsage() << ", Available Memory = "
                << pool.GetAvailableMemory() << hendl;
        }
    });
}

size_t MultiPoolAllocator::GetPoolIndex(size_t nBytes) const
{
    size_t index = 0;
    
    for (auto& pool : multiPool)
    {
        if (nBytes < pool.GetBlockSize())
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
    
    for (auto& pool : multiPool)
    {
        if (pool.IsMine(ptr))
        {
            return index;
        }
        
        ++index;
    }
    
    return index;
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
