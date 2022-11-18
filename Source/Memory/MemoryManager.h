// Created by mooming.go@gmail.com, 2022

#pragma once

#include "AllocatorID.h"
#include "AllocatorProxy.h"
#include "Config/BuildConfig.h"
#include "Container/AtomicStackView.h"
#include "Log/LogLevel.h"
#include "System/Types.h"
#include <atomic>
#include <functional>
#include <mutex>
#include <thread>


namespace std
{
struct source_location;
} // std

namespace HE
{

class MemoryManager final
{
public:
    using TId = TAllocatorID;
    using TAllocBytes = AllocatorProxy::TAllocBytes;
    using TDeallocBytes = AllocatorProxy::TDeallocBytes;
    using TLogFunc = std::function<void(std::ostream& out)>;

    static constexpr TId SystemAllocatorID = 0;
    static constexpr size_t MaxBaseMemory = 8'000'000'000;

private:
    struct UsageRecord final
    {
        size_t totalUsage = 0;
        size_t maxUsage = 0;
        size_t totalCapacity = 0;
        size_t maxCapacity = 0;
    };

    AllocatorProxy allocators[MaxNumAllocators];
    AtomicStackView<AllocatorProxy> proxyPool;

    std::mutex statsLock;
    size_t allocCount;
    size_t deallocCount;

    UsageRecord inlineUsage;
    UsageRecord usage;
    UsageRecord sysMemUsage;

    static thread_local TId ScopedAllocatorID;

public:
    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator= (const MemoryManager&) = delete;

    static MemoryManager& GetInstance();
    static TId GetCurrentAllocatorID();

public:
    MemoryManager();
    ~MemoryManager();

    const char* GetName() const;
    const char* GetName(TAllocatorID id) const;

    TId Register(const char* name, bool isInline, size_t capacity
        , TAllocBytes allocFunc, TDeallocBytes deallocFunc);

    std::lock_guard<std::mutex>&& AcquireStatsLock() { return std::move(std::lock_guard(statsLock)); }
    void Update(TId id, std::function<void(AllocatorProxy&)> func, const char* reason);

    void Deregister(TId id);
#ifdef PROFILE_ENABLED
    void Deregister(TId id, const std::source_location& srcLocation);
#endif // PROFILE_ENABLED

    void ReportAllocation(TId id, void* ptr, size_t requested, size_t allocated);
    void ReportDeallocation(TId id, void* ptr, size_t requested, size_t allocated);
    void ReportFallback(TId id, void* ptr, size_t requested);

    void* SysAllocate(size_t nBytes);
    void SysDeallocate(void* ptr, size_t nBytes);

    void* AllocateBytes(TId id, size_t nBytes);
    void DeallocateBytes(TId id, void* ptr, size_t nBytes);

    void* AllocateBytes(size_t nBytes);
    void DeallocateBytes(void* ptr, size_t nBytes);

    template <typename T>
    T* AllocateTypes(size_t n)
    {
        constexpr size_t sizeOfT = sizeof(T);
        auto nBytes = n * sizeOfT;
        auto ptr = AllocateBytes(nBytes);

        return static_cast<T*>(ptr);
    }

    template <typename T>
    void DeallocateTypes(T* ptr, size_t n)
    {
        auto nBytes = n * sizeof(T);
        DeallocateBytes(static_cast<void*>(ptr), nBytes);
    }

    template <typename Type, typename ... Types>
    inline Type* New(Types&& ... args)
    {
        auto ptr = AllocateBytes(sizeof(Type));
        auto tptr = new (ptr) Type(std::forward<Types>(args) ...);
        return tptr;
    }

    template <typename Type, typename ... Types>
    inline Type* NewArray(Index size, Types&& ... args)
    {
        auto ptr = reinterpret_cast<Type*>(AllocateBytes(sizeof(Type) * size));
        for (Index i = 0; i < size; ++i)
        {
            new (ptr[i]) Type(std::forward<Types>(args) ...);
        }

        return ptr;
    }

    template <typename Type>
    inline void Delete(Type* ptr)
    {
        ptr->~Type();
        DeallocateBytes(ptr, sizeof(Type));
    }

    template <typename Type>
    inline void DeleteArray(Type* ptr, size_t n)
    {
        for (size_t i = 0; i < n; ++i)
        {
            ptr[i]->~Type();
        }

        Deallocate(ptr, n);

        return ptr;
    }

    void Log(ELogLevel level, TLogFunc func);
    inline void LogWarning(TLogFunc func) { Log(ELogLevel::Warning, func); }
    inline void LogError(TLogFunc func) { Log(ELogLevel::Error, func); }

    inline auto& GetInlineUsage() const { return inlineUsage; }
    inline auto& GetUsage() const { return usage; }
    inline auto& GetSystemMemoryUsage() const { return sysMemUsage; }

private:
    inline bool IsValid(TAllocatorID id) const { return id >= 0 && id < MaxNumAllocators; }
    inline TId GetScopedAllocatorID() const { return ScopedAllocatorID; }

    void RegisterSystemAllocator();
    void DeregisterSystemAllocator();

    void SetScopedAllocatorID(TId id);

    friend class AllocatorScope;
};

} // HE
