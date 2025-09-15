// Created by mooming.go@gmail.com

#pragma once

#include <functional>
#include <mutex>
#include <span>
#include <thread>
#include <vector>
#include "AllocatorID.h"
#include "AllocatorProxy.h"
#include "Config/BuildConfig.h"
#include "Container/AtomicStackView.h"
#include "Core/Types.h"
#include "Log/LogLevel.h"
#include "MultiPoolConfigCache.h"
#include "PoolConfig.h"
#include "String/StaticStringID.h"

namespace hbe
{
	struct source_location;
	class Engine;

	class MemoryManager final
	{
	public:
		template<typename T>
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

		static thread_local TId ScopedAllocatorID;

		AllocatorProxy allocators[MaxNumAllocators];
		AtomicStackView<AllocatorProxy> proxyPool;

		std::mutex statsLock;
		size_t allocCount;
		size_t deallocCount;

		UsageRecord inlineUsage;
		UsageRecord usage;

		MultiPoolConfigCache multiPoolConfigCache;

#if PROFILE_ENABLED
		MultiPoolConfigCache multiPoolConfigLog;
#endif // PROFILE_ENABLED

	public:
		MemoryManager(const MemoryManager&) = delete;
		MemoryManager& operator=(const MemoryManager&) = delete;

		static StaticStringID GetMultiPoolConfigCacheFilePath();
		static MemoryManager& GetInstance();
		static TId GetCurrentAllocatorID();

	public:
		explicit MemoryManager(Engine& engine);
		~MemoryManager();

		void PostEngineInit();
		void PreEngineShutdown();

		static const char* GetName();
		const char* GetAllocatorName(TAllocatorID id) const;

		std::lock_guard<std::mutex> AcquireStatsLock() { return std::lock_guard(statsLock); }
		AllocatorProxy& GetAllocatorProxy(TId id);
		TId RegisterAllocator(void* allocator, const char* name, bool isInline, size_t capacity, TAllocBytes allocFunc,
							  TDeallocBytes deallocFunc);
		void DeregisterAllocator(TId id);

		void ReportAllocation(TId id, void* ptr, size_t requested, size_t allocated);
		void ReportDeallocation(TId id, void* ptr, size_t requested, size_t allocated);

		void* SysAllocate(size_t nBytes);
		void SysDeallocate(void* ptr, size_t nBytes);
		void* FallbackAllocate(TId id, TId parentId, size_t requested);

		void* Allocate(TId id, size_t nBytes);
		void Deallocate(TId id, void* ptr, size_t nBytes);

		void* Allocate(size_t nBytes);
		void Deallocate(void* ptr, size_t nBytes);

		bool IsLogEnabled(ELogLevel level) const;
		void Log(ELogLevel level, TLogFunc func) const;

		const MultiPoolAllocatorConfig& LookUpMultiPoolConfig(StaticStringID uniqueName) const;

#if PROFILE_ENABLED
		AllocStats GetAllocatorStat(TAllocatorID id);

		void DeregisterAllocator(TId id, const hbe::source_location& srcLocation);
		void ReportMultiPoolConfigutation(StaticStringID uniqueName, TPoolConfigs&& poolConfigs);
#endif // PROFILE_ENABLED

	public:
		void LogWarning(const TLogFunc& func) const { Log(ELogLevel::Warning, func); }
		void LogError(const TLogFunc& func) const { Log(ELogLevel::Error, func); }
		auto& GetInlineUsage() const { return inlineUsage; }
		auto& GetUsage() const { return usage; }

	public:
		template<typename T>
		T* AllocateByType(size_t n)
		{
			const auto nBytes = n * sizeof(T);
			auto ptr = Allocate(GetScopedAllocatorID(), nBytes);

			return static_cast<T*>(ptr);
		}

		template<typename T>
		void DeallocateTypes(T* ptr, size_t n)
		{
			const auto nBytes = n * sizeof(T);
			Deallocate(GetScopedAllocatorID(), static_cast<void*>(ptr), nBytes);
		}

		template<typename Type, typename... Types>
		Type* New(Types&&... args)
		{
			auto ptr = AllocateByType<Type>(1);
			auto tptr = new (ptr) Type(std::forward<Types>(args)...);
			return tptr;
		}

		template<typename Type, typename... Types>
		Type* NewArray(Index size, Types&&... args)
		{
			auto ptr = AllocateByType<Type>(size);

			for (Index i = 0; i < size; ++i)
			{
				new (&ptr[i]) Type(std::forward<Types>(args)...);
			}

			return ptr;
		}

		template<typename Type>
		void Delete(Type* ptr)
		{
			ptr->~Type();
			DeallocateTypes<Type>(ptr, 1);
		}

		template<typename Type>
		void DeleteArray(Type* ptr, size_t n)
		{
			for (size_t i = 0; i < n; ++i)
			{
				ptr[i].~Type();
			}

			DeallocateTypes<Type>(ptr, n);
		}

	private:
		static bool IsValid(TAllocatorID id) { return id >= 0 && id < MaxNumAllocators; }
		static TId GetScopedAllocatorID() { return ScopedAllocatorID; }

		void ReportFallback(TId id, void* ptr, size_t requested);
		void RegisterSystemAllocator();
		void DeregisterSystemAllocator();
		void LoadMultiPoolConfigs();
		void SaveMultiPoolConfigs();
		void SetScopedAllocatorID(TId id);

		friend class AllocatorScope;
	};

} // namespace hbe
