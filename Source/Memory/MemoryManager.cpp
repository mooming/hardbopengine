// Created by mooming.go@gmail.com, 2017

#include "MemoryManager.h"

#include "Engine.h"
#include "SystemAllocator.h"
#include "OSAL/Intrinsic.h"
#include "String/StringUtil.h"
#include "System/Debug.h"
#include <cstdint>


namespace HE
{

static_assert(MaxNumAllocators > 0, "MaxNumAllocators is invalid.");

thread_local MemoryManager::TId MemoryManager::ScopedAllocatorID = 0;
TDebugVariable<bool> DVarScopedAllocLogging = false;

static MemoryManager* MMgrInstance = nullptr;


MemoryManager& MemoryManager::GetInstance()
{
    FatalAssert(MMgrInstance != nullptr);
    return *MMgrInstance;
}

MemoryManager::TId MemoryManager::GetCurrentAllocatorID()
{
    return ScopedAllocatorID;
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
    
    static SystemAllocator<uint8_t> systemAllocator;
    
    auto allocFunc = [&sysAlloc = systemAllocator](size_t nBytes) -> void*
    {
        return sysAlloc.allocate(nBytes);
    };

    auto deallocFunc = [&sysAlloc = systemAllocator](void* ptr, size_t nBytes)
    {
        sysAlloc.deallocate(reinterpret_cast<uint8_t*>(ptr), nBytes);
    };

    Register("SystemAllocator", false, 0, allocFunc, deallocFunc);
    
    Assert(systemAllocator.GetID() == SystemAllocatorID);
    SetScopedAllocatorId(SystemAllocatorID);
}

MemoryManager::~MemoryManager()
{
    Assert(allocators[SystemAllocatorID].isValid);
    Deregister(SystemAllocatorID);
    MMgrInstance = nullptr;
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
            strncpy_s(allocator.name, name, LastIndex);
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
        
        Log(ELogLevel::Info
            , [funcName = __func__, name, i](auto& ls)
        {
            ls << "[" << funcName << "] Register Allocator (" << name
                << "), id = " << i;
        });

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
        
        Log(ELogLevel::Info
            , [funcName = __func__, name, i](auto& ls)
        {
            ls << "[" << funcName << "] Register Allocator(strong) (" << name
                << "), id = " << i;
        });
        
        return i;
    }
    
    Log(ELogLevel::Error
        , [funcName = __func__, name](auto& ls)
    {
        ls << "[" << funcName << "][" << name
            << "] failed to register an allocator.";
    });
    
    return InvalidAllocatorID;
}

void MemoryManager::Deregister(TId id)
{
    using namespace std;
    
    if (unlikely(!IsValid(id)))
    {
        Log(ELogLevel::Error
            , [funcName = __func__, id](auto& ls)
        {
            ls << "[" << funcName << "] Invalid allocator id(" << id
                << ") is provided.";
        });

        return;
    }
    
    auto& allocator = allocators[id];
    if (unlikely(!allocator.isValid))
    {
        Log(ELogLevel::Error
            , [funcName = __func__, id](auto& ls)
        {
            ls << "[" << funcName
                << "] Allocator(" << id << ") is not valid.";
        });
                          
        return;
    }

#ifdef __MEMORY_STATISTICS__
    Log(ELogLevel::Info
        , [funcName = __func__, id, &allocator](auto& ls)
    {
        ls << "[" << funcName
            << "][" << allocator.name << "] Allocator("
            << id << ") is deregistered.";
    });
#else // __MEMORY_STATISTICS__
    Log(ELogLevel::Info
        , [funcName = __func__, id](auto& ls)
    {
        ls << "[" << funcName
            << "] Allocator(" << id << ") is deregistered.";
    });
#endif // __MEMORY_STATISTICS__
    
    
    allocator.allocate = nullptr;
    allocator.deallocate = nullptr;
    
#ifdef __MEMORY_VERIFICATION__
    if (unlikely(allocator.threadId != std::this_thread::get_id()))
    {
        Log(ELogLevel::Error
            , [funcName = __func__, id](auto& ls)
        {
            ls << "[" << funcName
                << "] Allocator(" << id
                << ") Thread id is mismatched.";
        });
        
        debugBreak();
        
        return;
    }
#endif // __MEMORY_VERIFICATION__

#ifdef __MEMORY_STATISTICS__
    if (unlikely(allocator.usage > 0))
    {
        Log(ELogLevel::Warning
            , [funcName = __func__, &allocator, id](auto& ls)
        {
            ls << "[" << funcName << "] Allocator [" << allocator.name
                << "](" << id << ") Memory leak is detected!";
        });
        
        return;
    }
    
    if (unlikely(allocator.usage > 0))
    {
        Log(ELogLevel::Warning
            , [funcName = __func__, &allocator, id](auto& ls)
        {
            ls << "[" << funcName << "] Allocator [" << allocator.name
                << "](" << id << ") Memory leak is detected!";
        });
        
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
        Log(ELogLevel::Error
            , [id, ptr, requested, allocated](auto& ls)
        {
            ls << '[' << __func__ << "] Invalid allocator id(" << id
                << ") is provided. ptr = " << ptr
                << ", requested = " << requested
                << ", allocated = " << allocated;
        });
        
        return;
    }
    
    auto& allocator = allocators[id];
    
    Log(ELogLevel::Verbose
        , [&allocator, id, ptr, requested, allocated](auto& ls)
    {
        ls << '[' << static_cast<char*>(allocator.name) << "]["
            << id << "][ALLOC] ptr = " << ptr
            << ", requested = " << requested
            << ", allocated = " << allocated;
    });
            
    allocator.usage += allocated;
    
    if (unlikely(allocator.hasCapacity && allocator.usage > allocator.capacity))
    {
        Log(ELogLevel::Warning
            , [&allocator, ptr, requested, allocated](auto& ls)
        {
            ls << "[" << __func__ << "] Memory usage overflow. "
                << allocator.usage << " > "
                << allocator.capacity
                << ", ptr = " << ptr
                << ", requested = " << requested
                << ", allocated = " << allocated;
        });
        
        return;
    }
    
    if (allocator.isStack)
    {
        totalStackUsage += allocated;
        
        if (unlikely(allocator.hasCapacity && totalStackUsage > totalStackCapacity))
        {
            Log(ELogLevel::Warning
                , [this, funcName = __func__, ptr, requested, allocated](auto& ls)
            {
                ls << "[MemoryManager][" << funcName
                    << "] Stack usage overflow. "
                    << totalStackUsage << " > "
                    << totalStackCapacity
                    << ", ptr = " << ptr
                    << ", requested = " << requested
                    << ", allocated = " << allocated;
            });
        }
    }
    else
    {
        totalHeapUsage += allocated;
        
        if (unlikely(allocator.hasCapacity && totalHeapUsage > totalHeapCapacity))
        {
            Log(ELogLevel::Warning
                , [this, funcName = __func__, ptr, requested, allocated](auto& ls)
            {
                ls << '[' << funcName << "] Heap usage overflow. "
                    << totalHeapUsage << " > "
                    << totalHeapCapacity
                    << ", ptr = " << ptr
                    << ", requested = " << requested
                    << ", allocated = " << allocated;
            });
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
        Log(ELogLevel::Error, [funcName = __func__, id](auto& ls)
        {
            ls <<  '[' << funcName
                << "] Invalid allocator id(" << id
                << ") is provided.";
        });
        
        return;
    }
    
    auto& allocator = allocators[id];
    
    Log(ELogLevel::Verbose
        , [&allocator, id, ptr, requested, allocated](auto& ls)
    {
        ls << "[" << static_cast<char*>(allocator.name) << "]["
            << id << "][DEALLOC] "
            << " ptr = " << ptr
            << ", requested = " << requested
            << ", allocated = " << allocated;
    });
    
    if (unlikely(allocator.usage < allocated))
    {
        Log(ELogLevel::Error
            , [ptr, requested, allocated](auto& ls)
        {
            ls << '[' << __func__
                << "] Incorrect Memory usage. "
                << " ptr = " << ptr
                << ", requested = " << requested
                << ", allocated = " << allocated;
        });
    }
    
    allocator.usage -= allocated;
    
    if (allocator.isStack)
    {
        if (unlikely(totalStackUsage < allocated))
        {
            Log(ELogLevel::Error
                , [this, ptr, requested, allocated](auto& ls)
            {
                ls << '[' << __func__
                    << "] Incorrect Stack usage. "
                    << " ptr = " << ptr
                    << ", requested = " << requested
                    << ", allocated = " << allocated
                    << " > " << totalStackUsage;
            });
        }
        
        totalStackUsage -= allocated;
    }
    else
    {
        if (unlikely(totalHeapUsage < allocated))
        {
            Log(ELogLevel::Error
                , [this, funcName = __func__, ptr, requested, allocated](auto& ls )
            {
                ls <<  '[' << funcName
                    << "] Incorrect Heap usage."
                    << " ptr = " << ptr
                    << ", requested = " << requested
                    << ", allocated = " << allocated
                    << " > " << totalHeapUsage;
            });
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
        Log(ELogLevel::Error
            , [funcName = __func__, id](auto& ls )
        {
            ls << '[' << funcName << "] Invalid Scoped Allocator ID = "
                << id << ", the default allocator shall be used.";
        });
        
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
        Log(ELogLevel::Error
            , [funcName = __func__, id](auto& ls )
        {
            ls << '[' << funcName << "] Invalid Scoped Allocator ID = "
                << id << ", the default deallocate shall be used.";
        });
        
        id = 0;
    }

    auto& allocator = allocators[id];
    Assert(allocator.deallocate != nullptr
       , "[MemoryManager::Allocate][Error] No allocate function, ID = "
       , id);

    allocator.deallocate(ptr, nBytes);
}

void MemoryManager::Log(ELogLevel level, TLogFunc func)
{
#ifdef __MEMORY_LOGGING__
    if (static_cast<uint8_t>(level) < Config::MemLogLevel)
        return;
    
    auto& engine = Engine::Get();
    engine.Log(level, func);
#endif // __MEMORY_LOGGING__
}

void MemoryManager::SetScopedAllocatorId(TId id)
{
    using namespace std;
    
    if (unlikely(!IsValid(id)))
    {
        Log(ELogLevel::Error, [funcName = __func__, id](auto& ls)
        {
            ls << '[' << funcName << "] Invalid Scoped Allocator ID = "
                << id << ", the default deallocate shall be used.";
        });
        
        id = 0;
    }
    
    if (unlikely(DVarScopedAllocLogging))
    {
        Log(ELogLevel::Error, [funcName = __func__, id](auto& ls)
        {
            ls << '[' << funcName << "] Previous ID = "
                << id << ", New ID = " << id;
        });
    }
    
    ScopedAllocatorID = id;
}

} // HE
