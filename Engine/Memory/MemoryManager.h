// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <functional>
#include <mutex>
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

	/// @brief Centralized memory management system for the engine.
	/// @details This singleton class serves as the core memory controller, providing:
	/// - **Allocator Management**: Registration and retrieval of various allocators (System, Pool, Stack, etc.)
	///   via `TAllocatorID`.
	/// - **Scoped Allocation**: Support for thread-local allocation scopes using `ScopedAllocator`.
	/// - **Object Lifecycle**: Type-safe object creation (`New`, `NewArray`) and destruction (`Delete`, `DeleteArray`)
	///   with automatic constructor/destructor calls.
	/// - **Tracking & Statistics**: Real-time tracking of allocation/deallocation counts, usage, and capacity
	///   across all registered allocators.
	/// - **Fallthrough Mechanism**: A hierarchical fallback system for handling exhausted allocators.
	/// - **Configuration Persistence**: Loading and saving `MultiPool` configurations via `MultiPoolConfigCache`.
	/// - **Profiling Support**: Built-in hooks for memory investigation, logging, and detailed usage reporting
	///   when `PROFILE_ENABLED` is active.
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
		static thread_local TId scopedAllocatorID;

		static StaticStringID getMultiPoolConfigCacheFilePath();
		static MemoryManager& getInstance();
		static TId getCurrentAllocatorID();

		struct UsageRecord final
		{
			size_t allocCount = 0;
			size_t deallocCount = 0;
			size_t totalUsage = 0;
			size_t maxUsage = 0;
			size_t totalCapacity = 0;
			size_t maxCapacity = 0;
		};

	public:
		MemoryManager(const MemoryManager&) = delete;
		MemoryManager& operator=(const MemoryManager&) = delete;

		explicit MemoryManager(Engine& engine);
		~MemoryManager();

		void postEngineInit() noexcept;
		void preEngineShutdown() noexcept;

		static const char* getName();
		const char* getAllocatorName(TAllocatorID id) const;

		std::lock_guard<std::mutex> acquireStatsLock() { return std::lock_guard(statsLock); }
		AllocatorProxy& getAllocatorProxy(TId id);
		TId registerAllocator(void* allocator, const char* name, bool isInline, size_t capacity, TAllocBytes allocFunc,
							  TDeallocBytes deallocFunc);
		void deregisterAllocator(TId id);

		void reportAllocation(TId id, void* ptr, size_t requested, size_t allocated);
		void reportDeallocation(TId id, void* ptr, size_t requested, size_t allocated);

		void* sysAllocate(size_t nBytes);
		void sysDeallocate(void* ptr, size_t nBytes);
		void* fallbackAllocate(TId id, TId parentId, size_t requested);

		void* allocate(TId id, size_t nBytes);
		void Deallocate(TId id, void* ptr, size_t nBytes);

		void* allocate(size_t nBytes);
		void Deallocate(void* ptr, size_t nBytes);

		bool isLogEnabled(ELogLevel level) const;
		void log(ELogLevel level, TLogFunc func) const;

		const MultiPoolAllocatorConfig& lookUpMultiPoolConfig(StaticStringID uniqueName) const;

		void logWarning(const TLogFunc& func) const { log(ELogLevel::Warning, func); }
		void logError(const TLogFunc& func) const { log(ELogLevel::Error, func); }
		[[nodiscard]] auto& getInlineUsage() const { return inlineUsage; }
		[[nodiscard]] auto& getUsage() const { return usage; }

#if PROFILE_ENABLED
		AllocStats getAllocatorStat(TAllocatorID id);

		void deregisterAllocator(TId id, const hbe::source_location& srcLocation);
		void reportMultiPoolConfigutation(StaticStringID uniqueName, TPoolConfigs&& poolConfigs);
#endif // PROFILE_ENABLED

		template<typename T>
		T* allocateByType(size_t n)
		{
			const auto nBytes = n * sizeof(T);
			auto ptr = allocate(getScopedAllocatorID(), nBytes);

			return static_cast<T*>(ptr);
		}

		template<typename T>
		void deallocateTypes(T* ptr, size_t n)
		{
			const auto nBytes = n * sizeof(T);
			Deallocate(getScopedAllocatorID(), static_cast<void*>(ptr), nBytes);
		}

		template<typename Type, typename... Types>
		Type* New(Types&&... args)
		{
			auto ptr = allocateByType<Type>(1);
			auto tptr = new (ptr) Type(std::forward<Types>(args)...);
			return tptr;
		}

		template<typename Type, typename... Types>
		Type* newArray(Index size, Types&&... args)
		{
			auto ptr = allocateByType<Type>(size);

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
			deallocateTypes<Type>(ptr, 1);
		}

		template<typename Type>
		void deleteArray(Type* ptr, size_t n)
		{
			for (size_t i = 0; i < n; ++i)
			{
				ptr[i].~Type();
			}

			deallocateTypes<Type>(ptr, n);
		}

	private:
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

		[[nodiscard]] static bool IsValid(TAllocatorID id) { return id >= 0 && id < MaxNumAllocators; }
		[[nodiscard]] static TId getScopedAllocatorID() { return scopedAllocatorID; }

		void reportFallback(TId id, void* ptr, size_t requested);
		void registerSystemAllocator();
		void deregisterSystemAllocator();
		void loadMultiPoolConfigs();
		void saveMultiPoolConfigs();
		void setScopedAllocatorID(TId id);

		friend class AllocatorScope;
	};

} // namespace hbe
