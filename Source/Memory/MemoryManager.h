// Created by mooming.go@gmail.com

#pragma once

#include "AllocatorID.h"
#include "AllocatorProxy.h"
#include "Config/BuildConfig.h"
#include "Container/AtomicStackView.h"
#include "Log/LogLevel.h"
#include "MultiPoolConfigCache.h"
#include "PoolConfig.h"
#include "String/StaticStringID.h"
#include "System/Types.h"
#include <functional>
#include <mutex>
#include <span>
#include <thread>
#include <vector>

namespace std
{
    struct source_location;
} // namespace std

namespace HE
{

    class Engine;

    class MemoryManager final
    {
    public:
        template <typename T>
        using TVector = std::vector<T>;

        using TId = TAllocatorID;
        using TAllocBytes = AllocatorProxy::TAllocBytes;
        using TDeallocBytes = AllocatorProxy::TDeallocBytes;
        using TLogFunc = std::function<void(std::ostream& out)>;
        using TPoolConfigs = TVector<PoolConfig>;

        static constexpr TId SystemAllocatorID = 0;
        static constexpr size_t MaxBaseMemory = 8'000'000'000;

    private:
        struct UsageRecord final
        {
            size_t allocCount = 0;
            size_t deallocCount = 0;
            size_t totalUsage = 0;
            size_t maxUsage = 0;
            size_t totalCapacity = 0;
            size_t maxCapacity = 0;
        };

    private:
        static thread_local TId ScopedAllocatorID;

        AllocatorProxy allocators[MaxNumAllocators];
        AtomicStackView<AllocatorProxy> proxyPool;

        std::mutex statsLock;
        size_t allocCount;
        size_t deallocCount;

        UsageRecord inlineUsage;
        UsageRecord usage;

        MultiPoolConfigCache multiPoolConfigCache;

#ifdef PROFILE_ENABLED
        MultiPoolConfigCache multiPoolConfigLog;
#endif // PROFILE_ENABLED

    public:
        MemoryManager(const MemoryManager&) = delete;
        MemoryManager& operator=(const MemoryManager&) = delete;

        static StaticStringID GetMultiPoolConfigCacheFilePath();
        static MemoryManager& GetInstance();
        static TId GetCurrentAllocatorID();

    public:
        MemoryManager(Engine& engine);
        ~MemoryManager();

        void PostEngineInit();
        void PreEngineShutdown();

        const char* GetName() const;
        const char* GetName(TAllocatorID id) const;

        TId Register(const char* name, bool isInline, size_t capacity,
            TAllocBytes allocFunc, TDeallocBytes deallocFunc);

        std::lock_guard<std::mutex> AcquireStatsLock()
        {
            return std::lock_guard(statsLock);
        }

        void Update(TId id, std::function<void(AllocatorProxy&)> func,
            const char* reason);
        void Deregister(TId id);

        void ReportAllocation(
            TId id, void* ptr, size_t requested, size_t allocated);
        void ReportDeallocation(
            TId id, void* ptr, size_t requested, size_t allocated);
        void ReportFallback(TId id, void* ptr, size_t requested);

        void* SysAllocate(size_t nBytes);
        void SysDeallocate(void* ptr, size_t nBytes);

        void* AllocateBytes(TId id, size_t nBytes);
        void DeallocateBytes(TId id, void* ptr, size_t nBytes);

        void* AllocateBytes(size_t nBytes);
        void DeallocateBytes(void* ptr, size_t nBytes);

        bool IsLogEnabled(ELogLevel level) const;
        void Log(ELogLevel level, TLogFunc func) const;

        const MultiPoolAllocatorConfig& LookUpMultiPoolConfig(
            StaticStringID uniqueName) const;

#ifdef PROFILE_ENABLED
        AllocStats GetAllocatorStat(TAllocatorID id);

        void Deregister(TId id, const std::source_location& srcLocation);
        void ReportMultiPoolConfigutation(
            StaticStringID uniqueName, TPoolConfigs&& poolConfigs);
#endif // PROFILE_ENABLED

    public:
        inline void LogWarning(TLogFunc func) { Log(ELogLevel::Warning, func); }
        inline void LogError(TLogFunc func) { Log(ELogLevel::Error, func); }
        inline auto& GetInlineUsage() const { return inlineUsage; }
        inline auto& GetUsage() const { return usage; }

    public:
        template <typename T>
        T* AllocateTypes(size_t n)
        {
            const auto nBytes = n * sizeof(T);
            auto ptr = AllocateBytes(nBytes);

            return static_cast<T*>(ptr);
        }

        template <typename T>
        void DeallocateTypes(T* ptr, size_t n)
        {
            const auto nBytes = n * sizeof(T);
            DeallocateBytes(static_cast<void*>(ptr), nBytes);
        }

        template <typename Type, typename... Types>
        inline Type* New(Types&&... args)
        {
            auto ptr = AllocateTypes<Type>(1);
            auto tptr = new (ptr) Type(std::forward<Types>(args)...);
            return tptr;
        }

        template <typename Type, typename... Types>
        inline Type* NewArray(Index size, Types&&... args)
        {
            auto ptr = AllocateTypes<Type>(size);

            for (Index i = 0; i < size; ++i)
            {
                new (&ptr[i]) Type(std::forward<Types>(args)...);
            }

            return ptr;
        }

        template <typename Type>
        inline void Delete(Type* ptr)
        {
            ptr->~Type();
            DeallocateTypes<Type>(ptr, 1);
        }

        template <typename Type>
        inline void DeleteArray(Type* ptr, size_t n)
        {
            for (size_t i = 0; i < n; ++i)
            {
                ptr[i].~Type();
            }

            DeallocateTypes<Type>(ptr, n);
        }

    private:
        inline bool IsValid(TAllocatorID id) const
        {
            return id >= 0 && id < MaxNumAllocators;
        }
        inline TId GetScopedAllocatorID() const { return ScopedAllocatorID; }

        void RegisterSystemAllocator();
        void DeregisterSystemAllocator();

        void LoadMultiPoolConfigs();
        void SaveMultiPoolConfigs();

        void SetScopedAllocatorID(TId id);

        friend class AllocatorScope;
    };

} // namespace HE
