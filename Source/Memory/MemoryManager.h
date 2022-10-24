// Created by mooming.go@gmail.com, 2022

#pragma once

#include "AllocatorID.h"
#include "Config/BuildConfig.h"
#include "Log/LogLevel.h"
#include "System/Types.h"
#include <atomic>
#include <functional>
#include <thread>


namespace HE
{
    class MemoryManager final
    {
    public:
        using TId = TAllocatorID;
        using TAllocBytes = std::function<void*(size_t)>;
        using TDeallocBytes = std::function<void(void*, size_t)>;
        using TLogFunc = std::function<void(std::ostream& out)>;
        
        static constexpr TId SystemAllocatorID = 0;

    private:
        static constexpr TId NameBufferSize = 64;
        static constexpr size_t MaxBaseMemory = 8'000'000'000;

    private:
        struct AllocatorProxy final
        {
            std::atomic<bool> isValid = false;
            TAllocBytes allocate = nullptr;
            TDeallocBytes deallocate = nullptr;
            
#ifdef __MEMORY_STATISTICS__
            bool isStack = false;
            bool hasCapacity = false;
            size_t capacity = 0;
            size_t usage = 0;
            size_t fallbackCount = 0;
            size_t fallback = 0;
            char name[NameBufferSize] = "";
#endif // __MEMORY_STATISTICS__
            
#ifdef __MEMORY_VERIFICATION__
            std::thread::id threadId;
#endif // __MEMORY_VERIFICATION__
        };

        AllocatorProxy allocators[MaxNumAllocators];

        size_t allocCount;
        size_t deallocCount;
        
        size_t totalStackUsage;
        size_t totalHeapUsage;
        size_t totalSysHeapUsage;
        size_t totalStackCapacity;
        size_t totalHeapCapacity;

        size_t maxStackUsage;
        size_t maxHeapUsage;
        size_t maxSysHeapUsage;
        size_t maxStackCapacity;
        size_t maxHeapCapacity;

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

        TId Register(const char* name, bool isStack, size_t capacity
            , TAllocBytes allocFunc, TDeallocBytes deallocFunc);
        void Deregister(TId id);
        
        void ReportAllocation(TId id, void* ptr, size_t requested, size_t allocated);
        void ReportDeallocation(TId id, void* ptr, size_t requested, size_t allocated);
        void ReportFallback(TId id, void* ptr, size_t amount);

        void* SysAllocate(size_t nBytes);
        void SysDeallocate(void* ptr, size_t nBytes);
        
        void* Allocate(TId id, size_t nBytes);
        void Deallocate(TId id, void* ptr, size_t nBytes);

        void* Allocate(size_t nBytes);
        void Deallocate(void* ptr, size_t nBytes);

        template <typename T>
        T* Allocate(size_t n)
        {
            constexpr size_t sizeOfT = sizeof(T);
            auto nBytes = n * sizeOfT;
            auto ptr = Allocate(nBytes);
            
            return static_cast<T*>(ptr);
        }
        
        template <typename T>
        void Deallocate(T* ptr, size_t n)
        {
            auto nBytes = n * sizeof(T);
            Deallocate(static_cast<void*>(ptr), nBytes);
        }

        template <typename Type, typename ... Types>
        inline Type* New(Types&& ... args)
        {
            auto ptr = Allocate(sizeof(Type));
            auto tptr = new (ptr) Type(std::forward<Types>(args) ...);
            return tptr;
        }

        template <typename Type, typename ... Types>
        inline Type* NewArray(Index size, Types&& ... args)
        {
            auto ptr = reinterpret_cast<Type*>(Allocate(sizeof(Type) * size));
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
            Deallocate(ptr, 1);
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

        inline size_t GetTotalStackUsage() const { return totalStackUsage; }
        inline size_t GetTotalHeapUsage() const { return totalHeapUsage; }
        inline size_t GetTotalUsage() const { return GetTotalStackUsage() + GetTotalHeapUsage(); }

    private:
        inline bool IsValid(TAllocatorID id) const { return id >= 0 && id < MaxNumAllocators; }
        inline TId GetScopedAllocatorID() const { return ScopedAllocatorID; }
        void SetScopedAllocatorID(TId id);

        friend class AllocatorScope;
    };
}
