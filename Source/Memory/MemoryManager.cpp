// Created by mooming.go@gmail.com

#include "MemoryManager.h"

#include "../Engine/Engine.h"
#include "Config/ConfigParam.h"
#include "MultiPoolConfigCache.h"
#include "OSAL/Intrinsic.h"
#include "OSAL/OSInputOutput.h"
#include "Resource/Buffer.h"
#include "Resource/BufferUtil.h"
#include "String/StringUtil.h"
#include "System/Debug.h"
#include "SystemAllocator.h"
#include <cstdint>

namespace HE
{

static_assert(MaxNumAllocators > 0, "MaxNumAllocators is invalid.");

thread_local MemoryManager::TId MemoryManager::ScopedAllocatorID = 0;
TDebugVariable<bool> DVarScopedAllocLogging = false;

static MemoryManager* MMgrInstance = nullptr;

StaticStringID MemoryManager::GetMultiPoolConfigCacheFilePath()
{
    static const StaticString path(".multiPoolConfigCache.dat");
    return path.GetID();
}

MemoryManager& MemoryManager::GetInstance()
{
    FatalAssert(MMgrInstance != nullptr);
    return *MMgrInstance;
}

MemoryManager::TId MemoryManager::GetCurrentAllocatorID()
{
    return ScopedAllocatorID;
}

MemoryManager::MemoryManager(Engine& engine)
    : allocCount(0),
      deallocCount(0)
{
    Assert(MMgrInstance == nullptr);
    MMgrInstance = this;

    for (int i = 1; i < MaxNumAllocators; ++i)
    {
        auto& proxy = allocators[i];
        proxy.id = i;
        proxyPool.Push(proxy);
    }

    RegisterSystemAllocator();
    LoadMultiPoolConfigs();

    engine.SetMemoryManagerReady();
}

MemoryManager::~MemoryManager()
{
    if (MMgrInstance == nullptr)
    {
        return;
    }

    DeregisterSystemAllocator();
    MMgrInstance = nullptr;
}

void MemoryManager::PostEngineInit()
{
}

void MemoryManager::PreEngineShutdown()
{
#ifdef PROFILE_ENABLED
    SaveMultiPoolConfigs();
#endif // PROFILE_ENABLED
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

#ifdef PROFILE_ENABLED
    {
        auto& allocator = allocators[id];
        auto& stats = allocator.stats;

        return stats.name;
    }
#else  // PROFILE_ENABLED
    return "Unknown";
#endif // PROFILE_ENABLED
}

MemoryManager::TId MemoryManager::Register(const char* name, bool isInline,
    size_t capacity, TAllocBytes allocFunc, TDeallocBytes deallocFunc)
{
    if (name == nullptr)
    {
        name = "None";
    }

#ifdef PROFILE_ENABLED
    auto AddAllocator = [this, name, isInline, capacity, allocFunc,
                            deallocFunc](auto& allocator)
#else  // PROFILE_ENABLED
    auto AddAllocator = [allocFunc, deallocFunc](auto& allocator)
#endif // PROFILE_ENABLED
    {
        allocator.allocate = allocFunc;
        allocator.deallocate = deallocFunc;

#ifdef PROFILE_ENABLED
        {
            std::lock_guard lockScope(statsLock);

            auto& stats = allocator.stats;
            stats.OnRegister(name, isInline, capacity);

            auto& rec = isInline ? inlineUsage : usage;
            rec.totalCapacity += capacity;
            rec.maxCapacity = std::max(rec.maxCapacity, rec.totalCapacity);
        }
#endif // PROFILE_ENABLED

#ifdef __MEMORY_VERIFICATION__
        allocator.threadId = std::this_thread::get_id();
#endif // __MEMORY_VERIFICATION__
    };

    auto proxy = proxyPool.Pop();
    if (unlikely(proxy == nullptr))
    {
        Log(ELogLevel::FatalError, [funcName = __func__, name](auto& ls) {
            ls << "[" << funcName << "][" << name
               << "] failed to register an allocator.";
        });

        return InvalidAllocatorID;
    }

    auto& allocator = *proxy;
    auto id = allocator.id;

    AddAllocator(allocator);
    Assert(id != InvalidAllocatorID);

    Log(ELogLevel::Info, [funcName = __func__, name, id](auto& ls) {
        ls << "[" << funcName << "] " << name << "(" << id << ')';
    });

    return id;
}

void MemoryManager::Update(
    TId id, std::function<void(AllocatorProxy&)> func, const char* reason)
{
    if (unlikely(!IsValid(id)))
    {
        Log(ELogLevel::Error, [funcName = __func__, id](auto& ls) {
            ls << "[" << funcName << "] Invalid allocator id(" << id
               << ") is provided.";
        });

        return;
    }

    auto& proxy = allocators[id];
    func(proxy);

    Log(ELogLevel::Verbose,
        [funcName = __func__, &proxy, id, reason](auto& ls) {
            ls << "[" << funcName << "] " << proxy.GetName() << "(" << id
               << ") is updated. Reason = " << reason;
        });
}

void MemoryManager::Deregister(TId id)
{
    if (unlikely(!IsValid(id)))
    {
        Log(ELogLevel::Error, [funcName = __func__, id](auto& ls) {
            ls << "[" << funcName << "] Invalid allocator id(" << id
               << ") is provided.";
        });

        return;
    }

    auto& allocator = allocators[id];

#ifdef PROFILE_ENABLED
    auto& stats = allocator.stats;

    Log(ELogLevel::Info, [funcName = __func__, id, &stats](auto& ls) {
        ls << "[" << funcName << "] " << stats.name << "(" << id << ')';
    });

#else  // PROFILE_ENABLED
    Log(ELogLevel::Info, [funcName = __func__, id](auto& ls) {
        ls << "[" << funcName << "] ID(" << id << ')';
    });
#endif // PROFILE_ENABLED

    allocator.allocate = nullptr;
    allocator.deallocate = nullptr;

#ifdef __MEMORY_VERIFICATION__
    if (unlikely(allocator.threadId != std::this_thread::get_id()))
    {
        Log(ELogLevel::Error, [funcName = __func__, id](auto& ls) {
            ls << "[" << funcName << "] Allocator(" << id
               << ") Thread id is mismatched.";
        });

        debugBreak();

        return;
    }
#endif // __MEMORY_VERIFICATION__

#ifdef PROFILE_ENABLED
    if (unlikely(stats.usage > 0))
    {
        Log(ELogLevel::Warning, [funcName = __func__, &stats, id](auto& ls) {
            ls << "[" << funcName << "] Allocator [" << stats.name << "](" << id
               << ") Memory leak is detected! " << stats.usage << " / "
               << stats.capacity << " bytes";
        });

#ifdef __DEBUG__
        debugBreak();
#endif // __DEBUG__

        return;
    }

    if (unlikely(stats.usage > 0))
    {
        Log(ELogLevel::Warning, [funcName = __func__, &stats, id](auto& ls) {
            ls << "[" << funcName << "] Allocator [" << stats.name << "](" << id
               << ") Memory leak is detected!";
        });

        return;
    }

    {
        std::lock_guard lockScope(statsLock);

        auto& rec = stats.isInline ? inlineUsage : usage;
        rec.totalUsage -= stats.usage;
        rec.maxCapacity -= stats.capacity;
        stats.Reset();
    }

#endif // PROFILE_ENABLED

#ifdef __MEMORY_VERIFICATION__
    allocator.threadId = std::thread::id();
#endif // __MEMORY_VERIFICATION__

    proxyPool.Push(allocator);
}

#ifdef PROFILE_ENABLED
AllocStats MemoryManager::GetAllocatorStat(TAllocatorID id)
{
    if (unlikely(!IsValid(id)))
    {
        Log(ELogLevel::Error, [funcName = __func__, id](auto& ls) {
            ls << "[" << funcName << "] Invalid allocator id(" << id
               << ") is provided.";
        });

        static const AllocStats stats;
        return stats;
    }

    AllocStats stats;

    {
        std::lock_guard lockGuard(statsLock);

        auto& allocator = allocators[id];
        stats = allocator.stats;
    }

    return stats;
}

void MemoryManager::Deregister(TId id, const std::source_location& srcLoc)
{
    if (unlikely(!IsValid(id)))
    {
        Log(ELogLevel::Error, [funcName = __func__, id](auto& ls) {
            ls << "[" << funcName << "] Invalid allocator id(" << id
               << ") is provided.";
        });

        return;
    }

    {
        std::lock_guard lockGuard(statsLock);
        auto& allocator = allocators[id];
        auto& stats = allocator.stats;
        stats.Report();
    }

    Deregister(id);
}

void MemoryManager::ReportMultiPoolConfigutation(
    StaticStringID uniqueName, TPoolConfigs&& poolConfigs)
{
    auto& data = multiPoolConfigLog.GetData();
    data.emplace_back(uniqueName, std::move(poolConfigs));
}
#endif // PROFILE_ENABLED

void MemoryManager::ReportAllocation(
    TId id, void* ptr, size_t requested, size_t allocated)
{
#ifdef PROFILE_ENABLED
    using namespace std;

    if (unlikely(!IsValid(id)))
    {
        Log(ELogLevel::Error,
            [func = __func__, id, ptr, requested, allocated](auto& ls) {
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

    auto UpdateStats = [this, id, allocated, requested](
                           auto& outStats, auto& outRecord) {
        std::lock_guard lockScope(statsLock);
        ++allocCount;

        auto& allocator = allocators[id];
        auto& stats = allocator.stats;

        stats.usage += allocated;
        stats.maxUsage = std::max(stats.maxUsage, stats.usage);
        ++stats.allocCount;

        stats.totalRequested += requested;
        stats.maxRequested = std::max(requested, stats.maxRequested);

        auto& rec = stats.isInline ? inlineUsage : usage;
        ++rec.allocCount;
        rec.totalUsage += allocated;
        rec.maxUsage = std::max(rec.maxUsage, rec.totalUsage);

        outStats = stats;
        outRecord = rec;
    };

    AllocStats stats;
    UsageRecord rec;
    UpdateStats(stats, rec);

    if (unlikely(stats.usage > stats.capacity))
    {
        Log(ELogLevel::FatalError,
            [func = __func__, &stats, ptr, requested, allocated](auto& ls) {
                ls << '[' << func << "][" << stats.name
                   << "] Memory usage overflow. " << stats.usage << " > "
                   << stats.capacity << ", ptr = " << ptr
                   << ", requested = " << requested
                   << ", allocated = " << allocated;
            });

#ifdef __DEBUG__
        debugBreak();
#endif // __DEBUG__

        return;
    }

    if (unlikely(rec.totalUsage > rec.totalCapacity))
    {
        Log(ELogLevel::FatalError,
            [func = __func__, &stats, &rec, ptr, requested, allocated](
                auto& ls) {
                ls << "[MemoryManager][" << func << "][" << stats.name
                   << "] Usage overflow. " << rec.totalUsage
                   << " exceedes its limit " << rec.totalCapacity
                   << ", ptr = " << ptr << ", requested = " << requested
                   << ", allocated = " << allocated;
            });

#ifdef __DEBUG__
        debugBreak();
#endif // __DEBUG__
    }

    Log(ELogLevel::Info,
        [this, &stats, &rec, id, ptr, requested, allocated](auto& ls) {
            ls << "[Alloc(" << allocCount << ")][" << stats.name << '(' << id
               << ")] " << ptr << ", req = " << requested << '(' << allocated;

            auto PrintMemSize = [&ls](size_t size) {
                if (size < 1024)
                {
                    ls << size;
                    return;
                }

                ls << (size / 1024) << " KB";
            };

            ls << "), usage = ";
            PrintMemSize(stats.usage);

            ls << " / ";
            PrintMemSize(stats.capacity);

            ls << ", total usage = ";
            PrintMemSize(rec.totalUsage);
            ls << " / ";
            PrintMemSize(rec.totalCapacity);

            ls << ", count = " << stats.deallocCount;
            ls << " / " << stats.allocCount;
        });

    if (id == SystemAllocatorID)
    {
        auto& engine = Engine::Get();
        auto& statistics = engine.GetStatistics();
        statistics.ReportSysMemAlloc(allocated);
    }
#endif // PROFILE_ENABLED
}

void MemoryManager::ReportDeallocation(
    TId id, void* ptr, size_t requested, size_t allocated)
{
#ifdef PROFILE_ENABLED
    using namespace std;

    if (unlikely(!IsValid(id)))
    {
        Log(ELogLevel::Error, [func = __func__, id](auto& ls) {
            ls << '[' << func << "] Invalid allocator id(" << id
               << ") is provided.";
        });

        return;
    }

    std::lock_guard lockScope(statsLock);
    ++deallocCount;

    auto& allocator = allocators[id];
    auto& stats = allocator.stats;

    if (unlikely(stats.usage < allocated))
    {
        Log(ELogLevel::Error,
            [func = __func__, &stats, ptr, requested, allocated](auto& ls) {
                ls << '[' << func << "][" << stats.name
                   << "] Incorrect Memory usage. " << " ptr = " << ptr
                   << ", requested = " << requested
                   << ", allocated = " << allocated
                   << ", usage = " << stats.usage;
            });

#ifdef __DEBUG__
        debugBreak();
#endif // __DEBUG__
    }

    stats.usage -= allocated;
    ++stats.deallocCount;

    auto& rec = stats.isInline ? inlineUsage : usage;
    if (unlikely(rec.totalUsage < allocated))
    {
        Log(ELogLevel::Error,
            [func = __func__, &stats, &rec, ptr, requested, allocated](
                auto& ls) {
                ls << '[' << func << "][" << stats.name
                   << "] Incorrect memory usage." << " ptr = " << ptr
                   << ", requested = " << requested
                   << ", allocated = " << allocated << " > " << rec.totalUsage;
            });

#ifdef __DEBUG__
        debugBreak();
#endif // __DEBUG__

        return;
    }

    ++rec.deallocCount;
    rec.totalUsage -= allocated;

    if (id == SystemAllocatorID)
    {
        auto& engine = Engine::Get();
        auto& statistics = engine.GetStatistics();
        statistics.ReportSysMemDealloc(allocated);
    }

    Log(ELogLevel::Verbose,
        [this, &stats, &rec, id, ptr, requested, allocated](auto& ls) {
            ls << "[Dealloc(" << deallocCount << ")][" << stats.name << '('
               << id << ")] " << ptr << ", req = " << requested << '('
               << allocated;

            auto PrintMemSize = [&ls](size_t size) {
                if (size < 1024)
                {
                    ls << size;
                    return;
                }

                ls << (size / 1024) << " KB";
            };

            ls << "), usage = ";
            PrintMemSize(stats.usage);

            ls << " / ";
            PrintMemSize(stats.capacity);

            ls << ", total usage = ";
            PrintMemSize(rec.totalUsage);
            ls << " / ";
            PrintMemSize(rec.totalCapacity);

            ls << ", count = " << stats.deallocCount;
            ls << " / " << stats.allocCount;
        });
#endif // PROFILE_ENABLED
}

void MemoryManager::ReportFallback(TId id, void* ptr, size_t requested)
{
#ifdef PROFILE_ENABLED
    if (unlikely(!IsValid(id)))
    {
        Log(ELogLevel::Error, [funcName = __func__, id](auto& ls) {
            ls << '[' << funcName << "] Invalid allocator id(" << id
               << ") is provided.";
        });

#ifdef __DEBUG__
        debugBreak();
#endif // __DEBUG__

        return;
    }

    std::lock_guard lockScope(statsLock);

    auto& allocator = allocators[id];
    auto& stats = allocator.stats;

    ++stats.fallbackCount;
    stats.totalFallback += requested;
    stats.maxFallback = std::max(requested, stats.maxFallback);

    Log(ELogLevel::Verbose, [&stats, id, ptr, requested](auto& ls) {
        ls << "[" << stats.name << "][" << id << "][FallbackAlloc] "
           << " ptr = " << ptr << ", count = " << stats.fallbackCount
           << ", requested = " << requested;
    });
#endif // PROFILE_ENABLED
}

void* MemoryManager::SysAllocate(size_t nBytes)
{
    auto& allocator = allocators[SystemAllocatorID];
    if (unlikely(allocator.allocate == nullptr))
    {
        Log(ELogLevel::FatalError, [funcName = __func__](auto& ls) {
            ls << '[' << funcName << "] "
               << "SystemAllocator has no allocate function.";
        });

        return nullptr;
    }

    return allocator.allocate(nBytes);
}

void MemoryManager::SysDeallocate(void* ptr, size_t nBytes)
{
    auto& allocator = allocators[SystemAllocatorID];
    if (unlikely(allocator.deallocate == nullptr))
    {
        Log(ELogLevel::FatalError, [funcName = __func__](auto& ls) {
            ls << '[' << funcName << "] "
               << "SystemAllocator has no deallocate function.";
        });

        return;
    }

    allocator.deallocate(ptr, nBytes);
}

void* MemoryManager::AllocateBytes(TId id, size_t nBytes)
{
    if (unlikely(nBytes == 0))
    {
        return nullptr;
    }

    Assert(id != InvalidAllocatorID, "Memory allocation is not permitted.");

#ifdef __USE_SYSTEM_MALLOC__
    id = SystemAllocatorID;
#endif // __USE_SYSTEM_MALLOC__

    using namespace std;

    if (unlikely(!IsValid(id)))
    {
        Log(ELogLevel::Error, [funcName = __func__, id](auto& ls) {
            ls << '[' << funcName << "] Invalid Scoped Allocator ID = " << id
               << ", the default allocator shall be used.";
        });

        id = 0;
    }

    auto& allocator = allocators[id];
    Assert(allocator.allocate != nullptr, "[", GetName(), "::", __func__,
        "][Error] ", "No allocate function, ID = ", id);

    auto ptr = allocator.allocate(nBytes);
    Assert(ptr != nullptr, "Allocation Failed");

    return ptr;
}

void MemoryManager::DeallocateBytes(TId id, void* ptr, size_t nBytes)
{
    if (unlikely(ptr == nullptr))
    {
        Assert(nBytes == 0);
        return;
    }

    Assert(id != InvalidAllocatorID);

#ifdef __USE_SYSTEM_MALLOC__
    id = SystemAllocatorID;
#endif // __USE_SYSTEM_MALLOC__

    using namespace std;

    if (unlikely(!IsValid(id)))
    {
        Log(ELogLevel::Error, [funcName = __func__, id](auto& ls) {
            ls << '[' << funcName << "] Invalid Scoped Allocator ID = " << id
               << ", the default deallocate shall be used.";
        });

        id = 0;
    }

    auto& allocator = allocators[id];
    Assert(allocator.deallocate != nullptr,
        "[MemoryManager::Allocate][Error] No allocate function, ID = ", id);

    allocator.deallocate(ptr, nBytes);
}

void* MemoryManager::AllocateBytes(size_t nBytes)
{
    return AllocateBytes(GetScopedAllocatorID(), nBytes);
}

void MemoryManager::DeallocateBytes(void* ptr, size_t nBytes)
{
    DeallocateBytes(GetScopedAllocatorID(), ptr, nBytes);
}

bool MemoryManager::IsLogEnabled(ELogLevel level) const
{
#if __MEMORY_LOGGING__
    static TAtomicConfigParam<uint8_t> CPLogLevel("Log.Memory",
        "The Memory System Log Level",
        static_cast<uint8_t>(Config::MemLogLevel));

    if (static_cast<uint8_t>(level) < CPLogLevel.Get())
    {
        return false;
    }

    return true;
#else  // __MEMORY_LOGGING__
    return false;
#endif // __MEMORY_LOGGING__
}

void MemoryManager::Log(ELogLevel level, TLogFunc func) const
{
#if __MEMORY_LOGGING__
    if (!IsLogEnabled(level))
    {
        return;
    }

    auto& engine = Engine::Get();
    engine.Log(level, func);
#endif // __MEMORY_LOGGING__
}

const MultiPoolAllocatorConfig& MemoryManager::LookUpMultiPoolConfig(
    StaticStringID uniqueName) const
{
    static const MultiPoolAllocatorConfig null;

    auto& data = multiPoolConfigCache.GetData();
    const auto len = data.size();
    if (len <= 0)
    {
        return null;
    }

    size_t start = 0;
    size_t end = len;

    while (start < end)
    {
        auto mid = (start + end) / 2;
        Assert(start <= mid && mid < end);

        auto& item = data[mid];
        if (uniqueName == item.uniqueName)
        {
            Log(ELogLevel::Warning, [&item](auto& ls) {
                StaticString name(item.uniqueName);
                for (auto& config : item.configs)
                {
                    ls << "LookUpMultiPoolConfig: Found [" << name << "] ("
                       << config.blockSize << ", " << config.numberOfBlocks
                       << ")\n";
                }
            });

            return item;
        }

        if (uniqueName < item.uniqueName)
        {
            end = mid;
            continue;
        }

        // uniqueName > item.uniqueName
        start = mid + 1;
    }

    return null;
}

void MemoryManager::RegisterSystemAllocator()
{
    static SystemAllocator<uint8_t> systemAllocator;

    auto allocFunc = [&sysAlloc = systemAllocator](size_t nBytes) -> void* {
        return sysAlloc.allocate(nBytes);
    };

    auto deallocFunc = [&sysAlloc = systemAllocator](void* ptr, size_t nBytes) {
        sysAlloc.deallocate(reinterpret_cast<uint8_t*>(ptr), nBytes);
    };

    auto& allocator = allocators[SystemAllocatorID];
    allocator.allocate = allocFunc;
    allocator.deallocate = deallocFunc;

    Assert(usage.totalCapacity == 0);
    Assert(usage.maxCapacity == 0);
    usage.totalCapacity = Config::MemCapacity;
    usage.maxCapacity = Config::MemCapacity;
    Assert(systemAllocator.GetID() == SystemAllocatorID);
    SetScopedAllocatorID(SystemAllocatorID);

#ifdef PROFILE_ENABLED
    {
        std::lock_guard lockScope(statsLock);

        auto& stats = allocator.stats;

        stats.OnRegister("SystemAllocator", false, Config::MemCapacity);
        auto& rec = usage;
        rec.totalCapacity += Config::MemCapacity;
        rec.maxCapacity = std::max(rec.maxCapacity, rec.totalCapacity);
    }
#endif // PROFILE_ENABLED

#ifdef __MEMORY_VERIFICATION__
    allocator.threadId = std::this_thread::get_id();
#endif // __MEMORY_VERIFICATION__
}

void MemoryManager::DeregisterSystemAllocator()
{
    Deregister(SystemAllocatorID);
}

void MemoryManager::LoadMultiPoolConfigs()
{
    using namespace StringUtil;
    static const StaticString func(ToCompactMethodName(__PRETTY_FUNCTION__));

    StaticString path(GetMultiPoolConfigCacheFilePath());
    if (!OS::Exist(path))
    {
        Log(ELogLevel::Significant, [path](auto& ls) {
            ls << "Load MultiPoolConfig: Failed to find " << path;
        });

        return;
    }

    Log(ELogLevel::Significant,
        [path](auto& ls) { ls << "Load MultiPoolConfig: " << path; });

    auto buffer = BufferUtil::GetReadOnlyFileBuffer(path);
    if (buffer.GetData() == nullptr)
    {
        Log(ELogLevel::Error, [path](auto& ls) {
            ls << '[' << func << "] Failed to open the file " << path;
        });

        return;
    }

    if (!multiPoolConfigCache.Deserialize(buffer))
    {
        Log(ELogLevel::Error, [path](auto& ls) {
            ls << '[' << func << "] Failed to deserialize the file " << path;
        });

        return;
    }

#ifdef __DEBUG__
    constexpr auto logLevel = ELogLevel::Verbose;
    if (!IsLogEnabled(logLevel))
    {
        return;
    }

    auto& data = multiPoolConfigCache.GetData();
    size_t index = 0;

    for (auto& item : data)
    {
        StaticString itemName(item.uniqueName);
        auto& configs = item.configs;

        for (auto& config : configs)
        {
            Log(logLevel, [&index, itemName, &config](auto& ls) {
                ls << index++ << " : " << itemName << " (" << config.blockSize
                   << ", " << config.numberOfBlocks << ')';
            });
        }
    }
#endif // __DEBUG__
}

void MemoryManager::SaveMultiPoolConfigs()
{
    using namespace StringUtil;
    static const StaticString func(ToCompactMethodName(__PRETTY_FUNCTION__));

    auto GetOutputSize = [this]() -> size_t {
        auto buffer = BufferUtil::GenerateDummyBuffer();
        return multiPoolConfigLog.Serialize(buffer);
    };

    const size_t size = GetOutputSize();
    auto pathStrID = GetMultiPoolConfigCacheFilePath();
    StaticString path(pathStrID);

    Log(ELogLevel::Info, [size, path](auto& ls) {
        ls << '[' << func << "] MultiPoolConfig cache data(" << size
           << " bytes) shall be saved in " << path;
    });

    auto buffer = BufferUtil::GetWriteOnlyFileBuffer(path, size);
    auto result = multiPoolConfigLog.Serialize(buffer);

    if (result != size)
    {
        Log(ELogLevel::Error, [path](auto& ls) {
            ls << '[' << func << "] Failed to deserialize the file " << path;
        });

        return;
    }

#ifdef __DEBUG__
    constexpr auto logLevel = ELogLevel::Verbose;
    if (!IsLogEnabled(logLevel))
    {
        return;
    }

    auto& data = multiPoolConfigLog.GetData();
    size_t index = 0;

    for (auto& item : data)
    {
        StaticString itemName(item.uniqueName);
        auto& configs = item.configs;

        for (auto& config : configs)
        {
            Log(logLevel, [&index, itemName, &config](auto& ls) {
                ls << index++ << " : " << itemName << " (" << config.blockSize
                   << ", " << config.numberOfBlocks << ')';
            });
        }
    }
#endif // __DEBUG__
}

void MemoryManager::SetScopedAllocatorID(TId id)
{
    using namespace std;

    if (unlikely(id != InvalidAllocatorID && !IsValid(id)))
    {
        Log(ELogLevel::Error, [funcName = __func__, id](auto& ls) {
            ls << '[' << funcName << "] Invalid Scoped Allocator ID = " << id
               << ", the default deallocate shall be used.";
        });

        id = 0;
    }

    if (unlikely(DVarScopedAllocLogging))
    {
        Log(ELogLevel::Error, [funcName = __func__, id](auto& ls) {
            ls << '[' << funcName << "] Previous ID = " << id
               << ", New ID = " << id;
        });
    }

    ScopedAllocatorID = id;
}

} // namespace HE
