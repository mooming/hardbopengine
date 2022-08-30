// Created by mooming.go@gmail.com, 2022

#include "MultiPoolAllocator.h"

#include "HSTL/HStringStream.h"
#include "Log/Logger.h"
#include "Memory/MemoryManager.h"
#include <iostream>


namespace HE
{

MultiPoolAllocator::MultiPoolAllocator(const char* inName, TInitializerList list)
    : id(InvalidAllocatorID)
    , name(inName)
    , fallbackCount(0)
{
    using namespace std;;
    using namespace HSTL;
    
    HInlineVector<TPoolConfig, 256> vlist(list);
    std::sort(vlist.begin(), vlist.end());
    
    multiPool.reserve(vlist.size());

    for (auto& config : vlist)
    {
        HInlineString<2048> str;
        str += name;
        str += '_';
        str += config.first;
        str += '_';
        str += config.second;
        
        multiPool.emplace_back(str.c_str(), config.first, config.second);
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
        ls << endl << "## MultipoolAllocator(" << name <<  ") Usage ##" << endl;
        
        for (auto& pool : multiPool)
        {
            ls << '[' << pool.GetBlockSize() << "] Max Usage = " << pool.GetMaxUsage()
                << ", Available Memory = " << pool.GetAvailableMemory() << endl;
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
        if (pool.IsValid(ptr))
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

bool MultiPoolAllocatorTest::DoTest()
{
    int testCount = 0;
    int failCount = 0;
    
    auto log = Logger::Get(GetName(), ELogLevel::Info);
    
    {
        MultiPoolAllocator allocator("TestMultiPoolAlloc"
            , {{64, 1024}, {128, 1024}, {256, 1024}, {512, 1024}, {1024, 1024}, {2048, 1024}, {4096, 1024} });
        AllocatorScope scope(allocator);
    }
    
    ++testCount;
    
    {
        MultiPoolAllocator allocator("TestMultiPoolAlloc"
            , {{64, 1024}, {128, 1024}, {256, 1024}, {512, 1024}, {1024, 1024}, {2048, 1024}, {4096, 1024} });
        AllocatorScope scope(allocator);
        
        auto& mmgr = MemoryManager::GetInstance();
        mmgr.Allocate(0);
    }
    
    ++testCount;
    
    {
        MultiPoolAllocator allocator("TestMultiPoolAlloc"
            , {{64, 1024}, {128, 1024}, {256, 1024}, {512, 1024}, {1024, 1024}, {2048, 1024}, {4096, 1024} });
        AllocatorScope scope(allocator);
        
        auto& mmgr = MemoryManager::GetInstance();
        mmgr.Allocate(0);
        mmgr.Allocate(8);
        mmgr.Allocate(16);
        mmgr.Allocate(32);
        mmgr.Allocate(97);
        mmgr.Allocate(110);
        mmgr.Allocate(140);
        mmgr.Allocate(270);
        mmgr.Allocate(4032);
        mmgr.Allocate(5000);
        mmgr.Allocate(8000);
        
        if (allocator.GetFallbackCount() != 2)
        {
            ++failCount;
            log.OutError([&allocator](auto& ls)
            {
                ls << "Fallback count mismatched. FallbackCount = "
                    << allocator.GetFallbackCount() << ", but 2 expected.";
            });
        }
    }
    
    ++testCount;
    
    return failCount <= 0;
}

} // HE
#endif //__UNIT_TEST__
