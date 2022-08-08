// Created by mooming.go@gmail.com, 2017

#include "MemoryManager.h"

#include "SystemAllocator.h"
#include "OSAL/Intrinsic.h"
#include "String/StringUtil.h"
#include "System/Debug.h"


namespace HE
{

static_assert(MaxNumAllocators > 0, "MaxNumAllocators is invalid.");

thread_local MemoryManager::TId MemoryManager::ScopedAllocatorID = 0;
TDebugVariable<bool> DVarScopedAllocLogging = false;

static MemoryManager* MMgrInstance = nullptr;


MemoryManager& MemoryManager::GetInstance()
{
    Assert(MMgrInstance != nullptr);
    return *MMgrInstance;
}

MemoryManager::MemoryManager()
    : allocCount(0)
    , deallocCount(0)
    , totalStackUsage(0)
    , totalHeapUsage(0)
    , totalSysHeapUsage(0)
    , totalStackCapacity(0)
    , totalHeapCapacity(0)
{
    Assert(MMgrInstance == nullptr);
    MMgrInstance = this;
}

MemoryManager::~MemoryManager()
{
}

void MemoryManager::Initialize()
{
    static SystemAllocator<uint8_t> systemAllocator;
    Assert(systemAllocator.GetID() == SystemAllocatorID);
    SetScopedAllocatorId(SystemAllocatorID);
}

const char* MemoryManager::GetName() const
{
    return "MemoryManager";
}

MemoryManager::TId MemoryManager::Register(const char* name, bool isStack
    , size_t capacity, TAllocBytes allocFunc, TDeallocBytes deallocFunc)
{
#ifdef __MEMORY_STATISTICS__
    auto AddAllocator = [name, isStack, capacity, allocFunc, deallocFunc](auto& allocator)
#else // __MEMORY_STATISTICS__
    auto AddAllocator = [allocFunc, deallocFunc](auto& allocator)
#endif // __MEMORY_STATISTICS__
    {
        allocator.allocate = allocFunc;
        allocator.deallocate = deallocFunc;
        
#ifdef __MEMORY_STATISTICS__
        allocator.isStack = isStack;
        allocator.hasCapacity = capacity > 0;
        allocator.capacity = capacity;
        allocator.usage = 0;
        
        {
            constexpr int LastIndex = NameBufferSize - 1;
            strncpy(allocator.name, name, LastIndex);
            allocator.name[LastIndex] = '\0';
        }
#endif // __MEMORY_STATISTICS__
        
#ifdef __MEMORY_VERIFICATION__
        allocator.threadId = std::this_thread::get_id();
#endif // __MEMORY_VERIFICATION__
    };
    
    for (TId i = 0; i < MaxNumAllocators; ++i)
    {
        auto& allocator = allocators[i];
        auto& isValid = allocator.isValid;
        bool expected = false;
        
        if (!isValid.compare_exchange_weak(expected, true))
            continue;
        
        AddAllocator(allocator);
        Assert(allocator.isValid);
        
        return i;
    }
    
    for (TId i = 0; i < MaxNumAllocators; ++i)
    {
        auto& allocator = allocators[i];
        auto& isValid = allocator.isValid;
        bool expected = false;
        
        if (!isValid.compare_exchange_strong(expected, true))
            continue;
        
        AddAllocator(allocator);
        Assert(allocator.isValid);
        
        return i;
    }
    
    return MaxNumAllocators;
}

void MemoryManager::Deregister(TId id)
{
    using namespace std;
    
    if (unlikely(!IsValid(id)))
    {
        cerr << "[MemoryManager][" << __func__
            << "][Error] Invalid allocator id(" << id
            << ") is provided." << endl;
        
        DebugBreak();
        
        return;
    }
    
    auto& allocator = allocators[id];
    if (unlikely(!allocator.isValid))
    {
        cerr << "[MemoryManager][" << __func__
            << "][Error] Allocator(" << id << ") is not valid." << endl;
        
        Assert(false);
        
        return;
    }
    
    allocator.allocate = nullptr;
    allocator.deallocate = nullptr;
    
#ifdef __MEMORY_VERIFICATION__
    if (unlikely(allocator.threadId != std::this_thread::get_id()))
    {
        cerr << "[MemoryManager][" << __func__
            << "][Error] Allocator(" << id
            << ") Thread id is mismatched." << endl;
        
        Assert(false);
        
        return;
    }
#endif // __MEMORY_VERIFICATION__

#ifdef __MEMORY_STATISTICS__
    if (unlikely(allocator.usage > 0))
    {
        cerr << "[MemoryManager][" << __func__
            << "][Error] Allocator [" << allocator.name
            << "](" << id << ") Memory leak is detected!" << endl;
        
        Assert(false);
        
        return;
    }
    
    if (unlikely(allocator.usage > 0))
    {
        cerr << "[MemoryManager][" << __func__
            << "][Error] Allocator [" << allocator.name
            << "](" << id << ") Memory leak is detected!" << endl;
        
        Assert(false);
        
        return;
    }
    
    if (allocator.isStack)
    {
        totalStackCapacity -= allocator.capacity;
        totalStackUsage -= allocator.usage;
    }
    else
    {
        totalHeapCapacity -= allocator.capacity;
        totalHeapUsage -= allocator.usage;
    }
    
    allocator.isStack = false;
    allocator.hasCapacity = false;
    allocator.capacity = 0;
    allocator.usage = 0;
    allocator.name[0] = '\0';
#endif // __MEMORY_STATISTICS__
    
#ifdef __MEMORY_VERIFICATION__
    allocator.threadId = std::thread::id();
#endif // __MEMORY_VERIFICATION__
    
    allocator.isValid.store(false);
}

void MemoryManager::ReportAllocation(TId id, void* ptr
    , size_t requested, size_t allocated)
{
#ifdef __MEMORY_STATISTICS__
    using namespace std;
    
    if (unlikely(!IsValid(id)))
    {
        cerr << "[MemoryManager][" << __func__
            << "][Error] Invalid allocator id(" << id
            << ") is provided. ptr = " << ptr
            << ", requested = " << requested
            << ", allocated = " << allocated << endl;
        
        DebugBreak();
        
        return;
    }
    
    auto& allocator = allocators[id];
    
#ifdef __MEMORY_LOGGING__
    std::cout << "[" << static_cast<char*>(allocator.name) << "]["
        << id << "][ALLOC] ptr = " << ptr
        << ", requested = " << requested
        << ", allocated = " << allocated << endl;
#endif // __MEMORY_LOGGING__
            
    allocator.usage += allocated;
    
    if (unlikely(allocator.hasCapacity && allocator.usage > allocator.capacity))
    {
        cerr << "[MemoryManager][" << __func__
            << "][Error] Memory usage overflow. " << allocator.usage << " > "
            << allocator.capacity
            << ", ptr = " << ptr
            << ", requested = " << requested
            << ", allocated = " << allocated << endl;
        
        Assert(false);
        
        return;
    }
    
    if (allocator.isStack)
    {
        totalStackUsage += allocated;
        
        if (unlikely(allocator.hasCapacity && totalStackUsage > totalStackCapacity))
        {
            cerr << "[MemoryManager][" << __func__
                << "][Error] Stack usage overflow. "
                << totalStackUsage << " > "
                << totalStackCapacity
                << ", ptr = " << ptr
                << ", requested = " << requested
                << ", allocated = " << allocated << endl;
            
            Assert(false);
        }
    }
    else
    {
        totalHeapUsage += allocated;
        
        if (unlikely(allocator.hasCapacity && totalHeapUsage > totalHeapCapacity))
        {
            cerr << "[MemoryManager][" << __func__
                << "][Error] Heap usage overflow. "
                << totalHeapUsage << " > "
                << totalHeapCapacity
                << ", ptr = " << ptr
                << ", requested = " << requested
                << ", allocated = " << allocated << endl;
            
            Assert(false);
        }
    }
#endif // __MEMORY_STATISTICS__
}

void MemoryManager::ReportDeallocation(TId id, void* ptr
    , size_t requested, size_t allocated)
{
#ifdef __MEMORY_STATISTICS__
    using namespace std;
    
    if (unlikely(!IsValid(id)))
    {
        cerr << "[MemoryManager][" << __func__
            << "][Error] Invalid allocator id(" << id
            << ") is provided." << endl;
        return;
    }
    
    auto& allocator = allocators[id];
    
#ifdef __MEMORY_LOGGING__
    std::cout << "[" << static_cast<char*>(allocator.name) << "]["
        << id << "][DEALLOC] "
        << " ptr = " << ptr
        << ", requested = " << requested
        << ", allocated = " << allocated << endl;
#endif // __MEMORY_LOGGING__
    
    if (unlikely(allocator.usage < allocated))
    {
        cerr << "[MemoryManager][" << __func__
            << "][Error] Incorrect Memory usage. "
            << " ptr = " << ptr
            << ", requested = " << requested
            << ", allocated = " << allocated << endl;
        
        Assert(false);
    }
    
    allocator.usage -= allocated;
    
    if (allocator.isStack)
    {
        if (unlikely(totalStackUsage < allocated))
        {
            cerr << "[MemoryManager][" << __func__
                << "][Error] Incorrect Stack usage. "
                << " ptr = " << ptr
                << ", requested = " << requested
                << ", allocated = " << allocated
                << " > " << totalStackUsage << endl;
            
            Assert(false);
        }
        
        totalStackUsage -= allocated;
    }
    else
    {
        if (unlikely(totalHeapUsage < allocated))
        {
            cerr << "[MemoryManager][" << __func__
                << "][Error] Incorrect Heap usage."
                << " ptr = " << ptr
                << ", requested = " << requested
                << ", allocated = " << allocated
                << " > " << totalHeapUsage << endl;
            
            Assert(false);
        }
        
        totalHeapUsage -= allocated;
    }
#endif // __MEMORY_STATISTICS__
}

void* MemoryManager::SysAllocate(size_t nBytes)
{
    auto& allocator = allocators[SystemAllocatorID];
    
    Assert(allocator.isValid
           , "[", GetName(), "::",  __func__, "][Error] "
           , "SystemAllocator hasn't been set.");
    
    Assert(allocator.allocate != nullptr
           , "[", GetName(), "::", __func__, "][Error] "
           , "SystemAllocator has no allocate function.");

    return allocator.allocate(nBytes);
}

void MemoryManager::SysDeallocate(void* ptr, size_t nBytes)
{
    auto& allocator = allocators[SystemAllocatorID];
    
    Assert(allocator.isValid
           , "[", GetName(), "::",  __func__, "][Error] "
           , "SystemAllocator hasn't been set.");
    
    Assert(allocator.allocate != nullptr
           , "[", GetName(), "::", __func__, "][Error] "
           , "SystemAllocator has no allocate function.");

    allocator.deallocate(ptr, nBytes);
}

void* MemoryManager::Allocate(size_t nBytes)
{
    using namespace std;

    auto id = ScopedAllocatorID;
    if (unlikely(!IsValid(id)))
    {
        cerr << "[" << GetName() << "::" << __func__ << "][Error] "
            << "Invalid Scoped Allocator ID = "
            << ScopedAllocatorID
            << ", the default allocator shall be used." << endl;
        
        id = 0;
    }

    auto& allocator = allocators[id];
    Assert(allocator.isValid
           , "[", GetName(), "::", __func__, "][Error] "
           , "Invalid Allocator ID = ", id);
    
    Assert(allocator.allocate != nullptr
           , "[", GetName(), "::", __func__, "][Error] "
           , "No allocate function, ID = ", id);

    return allocator.allocate(nBytes);
}

void MemoryManager::Deallocate(void* ptr, size_t nBytes)
{
    using namespace std;

    auto id = ScopedAllocatorID;
    if (unlikely(!IsValid(id)))
    {
        cerr << "[MemorManager::Deallocate][Error] Invalid Scoped Allocator ID = "
            << ScopedAllocatorID
            << ", the default deallocate shall be used." << endl;
        
        id = 0;
    }

    auto& allocator = allocators[id];
    Assert(allocator.allocate != nullptr
       , "[MemoryManager::Allocate][Error] No allocate function, ID = "
       , id);

    allocator.deallocate(ptr, nBytes);
}

void MemoryManager::SetScopedAllocatorId(TId id)
{
    using namespace std;
    
    if (unlikely(!IsValid(id)))
    {
        cerr << "[MemorManager::SetScopedAllocatorId][Error] Invalid Scoped Allocator ID = "
            << ScopedAllocatorID
            << ", the default deallocate shall be used." << endl;
        
        id = 0;
    }
    
    if (unlikely(DVarScopedAllocLogging))
    {
        cout << "[MemorManager::SetScopedAllocatorId] Previous ID = "
            << ScopedAllocatorID
            << ", New ID = " << id << endl;
    }
    
    ScopedAllocatorID = id;
}

} // HE
