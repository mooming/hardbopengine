// Created by mooming.go@gmail.com, 2017

#include "MemoryManager.h"

#include "Engine.h"
#include "SystemAllocator.h"
#include "Config/ConfigParam.h"
#include "OSAL/Intrinsic.h"
#include "OSAL/SourceLocation.h"
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
{
    Assert(MMgrInstance == nullptr);
    MMgrInstance = this;
    
    RegisterSystemAllocator();
}

MemoryManager::~MemoryManager()
{
    if (MMgrInstance == nullptr)
        return;

    DeregisterSystemAllocator();
    MMgrInstance = nullptr;
}

const char* MemoryManager::GetName() const
{
    return "MemoryManager";
}

const char* MemoryManager::GetName(TAllocatorID id) const
{
    if (unlikely(!IsValid(id)))
    {
        return "Null";
    }

    return allocators[id].name;
}

MemoryManager::TId MemoryManager::Register(const char* name, bool isInline
    , size_t capacity, TAllocBytes allocFunc, TDeallocBytes deallocFunc)
{
    if (name == nullptr)
    {
        name = "None";
    }
    
#ifdef __MEMORY_STATISTICS__
    auto AddAllocator = [this, name, isInline, capacity, allocFunc, deallocFunc](auto& allocator)
#else // __MEMORY_STATISTICS__
    auto AddAllocator = [this, allocFunc, deallocFunc](auto& allocator)
#endif // __MEMORY_STATISTICS__
    {
        allocator.allocate = allocFunc;
        allocator.deallocate = deallocFunc;
        
#ifdef __MEMORY_STATISTICS__
        allocator.isInline = isInline;
        allocator.hasCapacity = capacity > 0;
        allocator.capacity = capacity;
        allocator.usage = 0;
        
        {
            constexpr int LastIndex = NameBufferSize - 1;
            for (int i = 0; i < NameBufferSize; ++i)
            {
                auto ch = name[i];
                allocator.name[i] = ch;
                
                if (ch == '\0')
                    break;
            }
            
            allocator.name[LastIndex] = '\0';
        }
#endif // __MEMORY_STATISTICS__

        {
            std::lock_guard lock(statLock);
            auto& rec = isInline ? inlineUsage : usage;
            rec.totalCapacity += capacity;
            rec.maxCapacity = std::max(rec.maxCapacity, rec.totalCapacity);
        }

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
            ls << "[" << funcName << "] " << name << "(" << i << ')';
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
            ls << "[" << funcName << "][Strong] " << name << "(" << i << ')';
        });
        
        return i;
    }
    
    Log(ELogLevel::Error
        , [funcName = __func__, name](auto& ls)
    {
        ls << "[" << funcName << "][" << name
            << "] failed to register an allocator.";
    });

    FatalAssert(false);
    
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
        ls << "[" << funcName << "] " << allocator.name << "(" << id << ')';
    });

#else // __MEMORY_STATISTICS__
    Log(ELogLevel::Info
        , [funcName = __func__, id, &allocator](auto& ls)
    {
        ls << "[" << funcName << "] ID(" << id << ')';
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
                << "](" << id << ") Memory leak is detected! "
                << allocator.usage << " / " << allocator.capacity
                << " bytes";
        });
        
#ifdef __DEBUG__
        debugBreak();
#endif // __DEBUG__

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

    {
        lock_guard lock(statLock);
        auto& rec = allocator.isInline ? inlineUsage : usage;
        rec.totalUsage -= allocator.usage;
        rec.maxCapacity -= allocator.capacity;
    }

    allocator.isInline = false;
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

#ifdef PROFILE_ENABLED
void MemoryManager::Deregister(TId id, const std::source_location& srcLoc)
{
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

    auto& engine = Engine::Get();
    auto& stat = engine.GetStatistics();
    stat.Report(static_cast<const char *>(allocator.name), srcLoc, allocator.maxUsage);

    Deregister(id);
}
#endif // PROFILE_ENABLED

void MemoryManager::ReportAllocation(TId id, void* ptr
    , size_t requested, size_t allocated)
{
#ifdef __MEMORY_STATISTICS__
    using namespace std;

    if (unlikely(!IsValid(id)))
    {
        Log(ELogLevel::Error
            , [func = __func__, id, ptr, requested, allocated](auto& ls)
        {
            ls << '[' << func << "] Invalid allocator id(" << id
                << ") is provided. ptr = " << ptr
                << ", requested = " << requested
                << ", allocated = " << allocated;
        });

#ifdef __DEBUG__
        debugBreak();
#endif // __DEBUG__

        return;
    }

    lock_guard lock(statLock);
    ++allocCount;

    auto& allocator = allocators[id];
    allocator.usage += allocated;
    allocator.maxUsage = std::max(allocator.maxUsage, allocator.usage);

    if (unlikely(allocator.hasCapacity && allocator.usage > allocator.capacity))
    {
        Log(ELogLevel::Warning
            , [func = __func__, &allocator, ptr, requested, allocated](auto& ls)
        {
            ls << '[' << func << "][" << static_cast<char*>(allocator.name)
                << "] Memory usage overflow. "
                << allocator.usage << " > "
                << allocator.capacity
                << ", ptr = " << ptr
                << ", requested = " << requested
                << ", allocated = " << allocated;
        });

#ifdef __DEBUG__
        debugBreak();
#endif // __DEBUG__

        return;
    }

    auto& rec = allocator.isInline ? inlineUsage : usage;
    rec.totalUsage += allocated;
    rec.maxUsage = std::max(rec.maxUsage, rec.totalUsage);

    if (unlikely(allocator.hasCapacity && rec.totalUsage > rec.totalCapacity))
    {
        Log(ELogLevel::FatalError
            , [func = __func__, &allocator, &rec, ptr, requested, allocated](auto& ls)
        {
            ls << "[MemoryManager][" << func
                << "][" << static_cast<char*>(allocator.name)
                << "] Inline usage overflow. "
                << rec.totalUsage << " exceedes its limit "
                << rec.totalCapacity
                << ", ptr = " << ptr
                << ", requested = " << requested
                << ", allocated = " << allocated;
        });

#ifdef __DEBUG__
        debugBreak();
#endif // __DEBUG__
    }

    Log(ELogLevel::Verbose
        , [this, &allocator, &rec, id, ptr, requested, allocated](auto& ls)
    {
        ls << "[Alloc(" << allocCount << ")]["
            << static_cast<char*>(allocator.name) << '('
            << id << ")] " << ptr
            << ", req = " << requested
            << '(' << allocated
            << "), usage = " << allocator.usage
            << " / " << allocator.capacity
            << ", total usage = " << rec.totalUsage
            << " / " << rec.totalCapacity;
    });
#endif // __MEMORY_STATISTICS__
}

void MemoryManager::ReportDeallocation(TId id, void* ptr
    , size_t requested, size_t allocated)
{
#ifdef __MEMORY_STATISTICS__
    using namespace std;

    if (unlikely(!IsValid(id)))
    {
        Log(ELogLevel::Error, [func = __func__, id](auto& ls)
        {
            ls <<  '[' << func
                << "] Invalid allocator id(" << id
                << ") is provided.";
        });
        
        return;
    }

    lock_guard lock(statLock);
    ++deallocCount;

    auto& allocator = allocators[id];
    
    if (unlikely(allocator.usage < allocated))
    {
        Log(ELogLevel::Error
            , [func = __func__, &allocator, ptr, requested, allocated](auto& ls)
        {
            ls << '[' << func
                << "][" << static_cast<char*>(allocator.name)
                << "] Incorrect Memory usage. "
                << " ptr = " << ptr
                << ", requested = " << requested
                << ", allocated = " << allocated
                << ", usage = " << allocator.usage;
        });

#ifdef __DEBUG__
        debugBreak();
#endif // __DEBUG__
    }
    
    allocator.usage -= allocated;

    auto& rec = allocator.isInline ? inlineUsage : usage;
    if (unlikely(rec.totalUsage < allocated))
    {
        Log(ELogLevel::Error
            , [func = __func__, &allocator, &rec, ptr, requested, allocated](auto& ls )
        {
            ls << '[' << func
                << "][" << static_cast<char*>(allocator.name)
                << "] Incorrect memory usage."
                << " ptr = " << ptr
                << ", requested = " << requested
                << ", allocated = " << allocated
                << " > " << rec.totalUsage;
        });

#ifdef __DEBUG__
        debugBreak();
#endif // __DEBUG__

        return;
    }

    rec.totalUsage -= allocated;

    Log(ELogLevel::Verbose
        , [this, &allocator, &rec, id, ptr, requested, allocated](auto& ls)
    {
        ls << "[Dealloc(" << deallocCount << ")]["
            << static_cast<char*>(allocator.name) << '('
            << id << ")] "  << ptr
            << ", req = " << requested
            << '(' << allocated
            << "), usage = " << allocator.usage
            << " / " << allocator.capacity
            << ", total usage = " << rec.totalUsage
            << " / " << rec.totalCapacity;
    });
#endif // __MEMORY_STATISTICS__
}

void MemoryManager::ReportFallback(TId id, void* ptr, size_t amount)
{
#ifdef __MEMORY_STATISTICS__
    if (unlikely(!IsValid(id)))
    {
        Log(ELogLevel::Error, [funcName = __func__, id](auto& ls)
        {
            ls <<  '[' << funcName
                << "] Invalid allocator id(" << id
                << ") is provided.";
        });

#ifdef __DEBUG__
        debugBreak();
#endif // __DEBUG__

        return;
    }

    std::lock_guard lock(statLock);
    auto& allocator = allocators[id];

    ++allocator.fallbackCount;
    allocator.fallback += amount;

    Log(ELogLevel::Verbose
        , [&allocator, id, ptr, amount](auto& ls)
    {
        ls << "[" << static_cast<char*>(allocator.name) << "]["
            << id << "][FallbackAlloc] "
            << " ptr = " << ptr
            << ", count = " << allocator.fallbackCount
            << ", amount = " << amount
            << " / " << allocator.fallback;
    });
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

#ifdef __MEMORY_VERIFICATION__
    Assert(allocator.isValid
           , "[", GetName(), "::",  __func__, "][Error] "
           , "SystemAllocator hasn't been set.");
    
    Assert(allocator.allocate != nullptr
           , "[", GetName(), "::", __func__, "][Error] "
           , "SystemAllocator has no allocate function.");
#endif // __MEMORY_VERIFICATION__
    allocator.deallocate(ptr, nBytes);
}

void* MemoryManager::Allocate(TId id, size_t nBytes)
{
    Assert(id != InvalidAllocatorID, "Memory allocation is not permitted.");

#ifdef __USE_SYSTEM_MALLOC__
    id = SystemAllocatorID;
#endif // __USE_SYSTEM_MALLOC__
    
    using namespace std;

    if (unlikely(!IsValid(id)))
    {
        Log(ELogLevel::Error
            , [funcName = __func__, id](auto& ls)
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

    auto ptr = allocator.allocate(nBytes);
    Assert(ptr != nullptr, "Allocation Failed");

    return ptr;
}

void MemoryManager::Deallocate(TId id, void* ptr, size_t nBytes)
{
    Assert(id != InvalidAllocatorID);
    
#ifdef __USE_SYSTEM_MALLOC__
    id = SystemAllocatorID;
#endif // __USE_SYSTEM_MALLOC__

    using namespace std;

    if (unlikely(!IsValid(id)))
    {
        Log(ELogLevel::Error
            , [funcName = __func__, id](auto& ls)
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

void* MemoryManager::Allocate(size_t nBytes)
{
    return Allocate(GetScopedAllocatorID(), nBytes);
}

void MemoryManager::Deallocate(void* ptr, size_t nBytes)
{
    Deallocate(GetScopedAllocatorID(), ptr, nBytes);
}

void MemoryManager::Log(ELogLevel level, TLogFunc func)
{
#ifdef __MEMORY_LOGGING__
    static TAtomicConfigParam<uint8_t> CPLogLevel("Log.Memory"
       , "The Memory System Log Level"
       , static_cast<uint8_t>(Config::MemLogLevel));

    if (static_cast<uint8_t>(level) < CPLogLevel.Get())
        return;

    auto& engine = Engine::Get();
    engine.Log(level, func);
#endif // __MEMORY_LOGGING__
}

void MemoryManager::RegisterSystemAllocator()
{
    static SystemAllocator<uint8_t> systemAllocator;

    auto allocFunc = [&sysAlloc = systemAllocator](size_t nBytes) -> void*
    {
        return sysAlloc.allocate(nBytes);
    };

    auto deallocFunc = [&sysAlloc = systemAllocator](void* ptr, size_t nBytes)
    {
        sysAlloc.deallocate(reinterpret_cast<uint8_t*>(ptr), nBytes);
    };

    Register("SystemAllocator", false, Config::MemCapacity, allocFunc, deallocFunc);

    Assert(sysMemUsage.totalCapacity == 0);
    Assert(sysMemUsage.maxCapacity == 0);
    sysMemUsage.totalCapacity = Config::MemCapacity;
    sysMemUsage.maxCapacity = Config::MemCapacity;

    Assert(systemAllocator.GetID() == SystemAllocatorID);
    SetScopedAllocatorID(SystemAllocatorID);
}

void MemoryManager::DeregisterSystemAllocator()
{
    Assert(allocators[SystemAllocatorID].isValid);
    Deregister(SystemAllocatorID);

    sysMemUsage.totalCapacity = 0;
    sysMemUsage.maxCapacity = 0;
}

void MemoryManager::SetScopedAllocatorID(TId id)
{
    using namespace std;
    
    if (unlikely(id != InvalidAllocatorID && !IsValid(id)))
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
