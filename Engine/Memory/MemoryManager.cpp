// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "MemoryManager.h"

#include "Config/ConfigParam.h"
#include "Core/Debug.h"
#include "Engine/Engine.h"
#include "MultiPoolConfigCache.h"
#include "OSAL/Intrinsic.h"
#include "OSAL/OSInputOutput.h"
#include "Resource/Buffer.h"
#include "Resource/BufferUtil.h"
#include "String/StringUtil.h"
#include "SystemAllocator.h"


namespace hbe
{

	static_assert(MaxNumAllocators > 0, "MaxNumAllocators is invalid.");

	thread_local MemoryManager::TId MemoryManager::scopedAllocatorID = 0;
	TDebugVariable<bool> dVarScopedAllocLogging = false;

	static MemoryManager* mmgrInstance = nullptr;

	StaticStringID MemoryManager::getMultiPoolConfigCacheFilePath()
	{
		static const StaticString path(MULTIPOOL_ALLOC_LOG);
		return path.getID();
	}

	MemoryManager& MemoryManager::getInstance()
	{
		fatalAssert(mmgrInstance != nullptr);
		return *mmgrInstance;
	}

	MemoryManager::TId MemoryManager::getCurrentAllocatorID() { return scopedAllocatorID; }

	MemoryManager::MemoryManager(Engine& engine) : allocCount(0), deallocCount(0)
	{
		Assert(mmgrInstance == nullptr);
		mmgrInstance = this;

		for (int i = 1; i < MaxNumAllocators; ++i)
		{
			auto& proxy = allocators[i];
			proxy.id = i;
			proxyPool.push(proxy);
		}

		registerSystemAllocator();
		loadMultiPoolConfigs();

		engine.setMemoryManagerReady();
	}

	MemoryManager::~MemoryManager()
	{
		if (mmgrInstance == nullptr)
		{
			return;
		}

		deregisterSystemAllocator();
		mmgrInstance = nullptr;
	}

	void MemoryManager::postEngineInit() noexcept {}

	void MemoryManager::preEngineShutdown() noexcept
	{
#if PROFILE_ENABLED
		saveMultiPoolConfigs();
#endif // PROFILE_ENABLED
	}

	const char* MemoryManager::getName() { return "MemoryManager"; }

	// Return name of an allocator with the given allocator ID.
	// It returns a valid name if "PROFILE_ENABLED" is on.
	const char* MemoryManager::getAllocatorName(TAllocatorID id) const
	{
		if (unlikely(!IsValid(id)))
		{
			return "Null";
		}

#if PROFILE_ENABLED
		{
			auto& allocator = allocators[id];
			auto& stats = allocator.stats;

			return stats.name;
		}
#else // PROFILE_ENABLED
		return "Unknown";
#endif // PROFILE_ENABLED
	}

	// Register a allocator
	MemoryManager::TId MemoryManager::registerAllocator(void* allocator, const char* name, bool isInline,
														size_t capacity, TAllocBytes allocFunc,
														TDeallocBytes deallocFunc)
	{
		if (name == nullptr)
		{
			name = "None";
		}

#if PROFILE_ENABLED
		auto AddAllocator = [this, allocator, name, isInline, capacity, allocFunc, deallocFunc](auto& allocProxy)
#else // PROFILE_ENABLED
		auto AddAllocator = [allocator, allocFunc, deallocFunc](auto& allocProxy)
#endif // PROFILE_ENABLED
		{
			allocProxy.allocator = allocator;
			allocProxy.allocate = allocFunc;
			allocProxy.deallocate = deallocFunc;

#if PROFILE_ENABLED
			{
				std::lock_guard lockScope(statsLock);

				auto& stats = allocProxy.stats;
				stats.onRegister(name, isInline, capacity);

				auto& rec = isInline ? inlineUsage : usage;
				rec.totalCapacity += capacity;
				rec.maxCapacity = std::max(rec.maxCapacity, rec.totalCapacity);
			}
#endif // PROFILE_ENABLED

#if MEMORY_VERIFICATION_ENABLED
			allocProxy.threadId = std::this_thread::get_id();
#endif // MEMORY_VERIFICATION_ENABLED
		};

		auto allocatorProxyPtr = proxyPool.pop();
		if (unlikely(allocatorProxyPtr == nullptr))
		{
			log(ELogLevel::FatalError, [funcName = __func__, name](auto& ls)
			{ ls << "[" << funcName << "][" << name << "] failed to register an allocator."; });

			return InvalidAllocatorID;
		}

		auto& allocatorProxy = *allocatorProxyPtr;
		auto id = allocatorProxy.id;

		AddAllocator(allocatorProxy);
		Assert(id != InvalidAllocatorID);

		log(ELogLevel::Info,
			[funcName = __func__, name, id](auto& ls) { ls << "[" << funcName << "] " << name << "(" << id << ')'; });

		return id;
	}

	AllocatorProxy& MemoryManager::getAllocatorProxy(TId id)
	{
		if (unlikely(!IsValid(id)))
		{
			log(ELogLevel::Error, [funcName = __func__, id](auto& ls)
			{ ls << "[" << funcName << "] Invalid allocator id(" << id << ") is provided."; });

			// Return a dummy proxy
			static AllocatorProxy dummy;
			new (&dummy) AllocatorProxy();

			return dummy;
		}

		return allocators[id];
	}

	void MemoryManager::deregisterAllocator(TId id)
	{
		if (unlikely(!IsValid(id)))
		{
			log(ELogLevel::Error, [funcName = __func__, id](auto& ls)
			{ ls << "[" << funcName << "] Invalid allocator id(" << id << ") is provided."; });

			return;
		}

		auto& allocator = allocators[id];

#if PROFILE_ENABLED
		auto& stats = allocator.stats;
		log(ELogLevel::Info, [funcName = __func__, id, &stats](auto& ls)
		{ ls << "[" << funcName << "] " << stats.name << "(" << id << ')'; });

#else // PROFILE_ENABLED
		log(ELogLevel::Info, [funcName = __func__, id](auto& ls) { ls << "[" << funcName << "] ID(" << id << ')'; });
#endif // PROFILE_ENABLED

		allocator.allocate = nullptr;
		allocator.deallocate = nullptr;

#if MEMORY_VERIFICATION_ENABLED
		if (unlikely(allocator.threadId != std::this_thread::get_id()))
		{
			log(ELogLevel::Error, [funcName = __func__, id](auto& ls)
			{ ls << "[" << funcName << "] Allocator(" << id << ") Thread id is mismatched."; });

#ifdef __DEBUG__
			debugBreak();
#endif // __DEBUG__

			return;
		}
#endif // MEMORY_VERIFICATION_ENABLED

#if PROFILE_ENABLED
		if (unlikely(stats.usage > 0))
		{
			log(ELogLevel::Warning, [funcName = __func__, &stats, id](auto& ls)
			{
				ls << "[" << funcName << "] Allocator [" << stats.name << "](" << id << ") Memory leak is detected! "
				   << stats.usage << " / " << stats.capacity << " bytes";
			});

#ifdef __DEBUG__
			debugBreak();
#endif // __DEBUG__

			return;
		}

		if (unlikely(stats.usage > 0))
		{
			log(ELogLevel::Warning, [funcName = __func__, &stats, id](auto& ls)
			{ ls << "[" << funcName << "] Allocator [" << stats.name << "](" << id << ") Memory leak is detected!"; });

			return;
		}

		{
			std::lock_guard lockScope(statsLock);

			auto& rec = stats.isInline ? inlineUsage : usage;
			rec.totalUsage -= stats.usage;
			rec.maxCapacity -= stats.capacity;
			stats.reset();
		}

#endif // PROFILE_ENABLED

#if MEMORY_VERIFICATION_ENABLED
		allocator.threadId = std::thread::id();
#endif // MEMORY_VERIFICATION_ENABLED

		proxyPool.push(allocator);
	}

#if PROFILE_ENABLED
	AllocStats MemoryManager::getAllocatorStat(TAllocatorID id)
	{
		if (unlikely(!IsValid(id)))
		{
			log(ELogLevel::Error, [funcName = __func__, id](auto& ls)
			{ ls << "[" << funcName << "] Invalid allocator id(" << id << ") is provided."; });

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

	void MemoryManager::deregisterAllocator(TId id, const hbe::source_location& srcLoc)
	{
		if (unlikely(!IsValid(id)))
		{
			log(ELogLevel::Error, [funcName = __func__, id](auto& ls)
			{ ls << "[" << funcName << "] Invalid allocator id(" << id << ") is provided."; });

			return;
		}

		{
			std::lock_guard lockGuard(statsLock);
			auto& allocator = allocators[id];
			auto& stats = allocator.stats;
			stats.report();
		}

		deregisterAllocator(id);
	}

	void MemoryManager::reportMultiPoolConfigutation(StaticStringID uniqueName, TPoolConfigs&& poolConfigs)
	{
		auto& data = multiPoolConfigLog.getData();
		data.emplace_back(uniqueName, std::move(poolConfigs));
	}
#endif // PROFILE_ENABLED

	void MemoryManager::reportAllocation(TId id, void* ptr, size_t requested, size_t allocated)
	{
#if PROFILE_ENABLED
		using namespace std;

		if (unlikely(!IsValid(id)))
		{
			log(ELogLevel::Error, [func = __func__, id, ptr, requested, allocated](auto& ls)
			{
				ls << '[' << func << "] Invalid allocator id(" << id << ") is provided. ptr = " << ptr
				   << ", requested = " << requested << ", allocated = " << allocated;
			});

#ifdef __DEBUG__
			debugBreak();
#endif // __DEBUG__

			return;
		}

		auto UpdateStats = [this, id, allocated, requested](auto& outStats, auto& outRecord)
		{
			std::lock_guard lockScope(statsLock);
			++allocCount;

			auto& allocator = allocators[id];
			auto& stats = allocator.stats;

			stats.usage += allocated;
			stats.maxUsage = std::max(stats.maxUsage, stats.usage);
			++stats.allocCount;

			stats.totalRequested += requested;
			stats.maxRequested = std::max(requested, stats.maxRequested);

			auto& usageRecord = stats.isInline ? inlineUsage : usage;
			++usageRecord.allocCount;
			usageRecord.totalUsage += allocated;
			usageRecord.maxUsage = std::max(usageRecord.maxUsage, usageRecord.totalUsage);

			outStats = stats;
			outRecord = usageRecord;
		};

		AllocStats stats;
		UsageRecord rec;
		UpdateStats(stats, rec);

		if (unlikely(stats.usage > stats.capacity))
		{
			log(ELogLevel::FatalError, [func = __func__, &stats, ptr, requested, allocated](auto& ls)
			{
				ls << '[' << func << "][" << stats.name << "] Memory usage overflow. " << stats.usage << " > "
				   << stats.capacity << ", ptr = " << ptr << ", requested = " << requested
				   << ", allocated = " << allocated;
			});

#ifdef __DEBUG__
			debugBreak();
#endif // __DEBUG__

			return;
		}

		if (unlikely(rec.totalUsage > rec.totalCapacity))
		{
			log(ELogLevel::FatalError, [func = __func__, &stats, &rec, ptr, requested, allocated](auto& ls)
			{
				ls << "[MemoryManager][" << func << "][" << stats.name << "] Usage overflow. " << rec.totalUsage
				   << " exceedes its limit " << rec.totalCapacity << ", ptr = " << ptr << ", requested = " << requested
				   << ", allocated = " << allocated;
			});

#ifdef __DEBUG__
			debugBreak();
#endif // __DEBUG__
		}

		log(ELogLevel::Info, [this, &stats, &rec, id, ptr, requested, allocated](auto& ls)
		{
			ls << "[Alloc:" << allocCount << "][Allocator: " << stats.name << '(' << id << ")], PTR = " << ptr
			   << ", Requested = " << requested << ", Allocated = " << allocated;

			auto PrintMemSize = [&ls](size_t size)
			{
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
			auto& engine = Engine::get();
			auto& statistics = engine.getStatistics();
			statistics.reportSysMemAlloc(allocated);
		}
#endif // PROFILE_ENABLED
	}

	void MemoryManager::reportDeallocation(TId id, void* ptr, size_t requested, size_t allocated)
	{
#if PROFILE_ENABLED
		using namespace std;

		if (unlikely(!IsValid(id)))
		{
			log(ELogLevel::Error, [func = __func__, id](auto& ls)
			{ ls << '[' << func << "] Invalid allocator id(" << id << ") is provided."; });

			return;
		}

		std::lock_guard lockScope(statsLock);
		++deallocCount;

		auto& allocator = allocators[id];
		auto& stats = allocator.stats;

		if (unlikely(stats.usage < allocated))
		{
			log(ELogLevel::Error, [func = __func__, &stats, ptr, requested, allocated](auto& ls)
			{
				ls << '[' << func << "][" << stats.name << "] Incorrect Memory usage. " << " ptr = " << ptr
				   << ", requested = " << requested << ", allocated = " << allocated << ", usage = " << stats.usage;
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
			log(ELogLevel::Error, [func = __func__, &stats, &rec, ptr, requested, allocated](auto& ls)
			{
				ls << '[' << func << "][" << stats.name << "] Incorrect memory usage." << " ptr = " << ptr
				   << ", requested = " << requested << ", allocated = " << allocated << " > " << rec.totalUsage;
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
			auto& engine = Engine::get();
			auto& statistics = engine.getStatistics();
			statistics.reportSysMemDealloc(allocated);
		}

		log(ELogLevel::Info, [this, &stats, &rec, id, ptr, requested, allocated](auto& ls)
		{
			ls << "[Dealloc(" << deallocCount << ")][" << stats.name << '(' << id << ")] " << ptr
			   << ", req = " << requested << '(' << allocated;

			auto PrintMemSize = [&ls](size_t size)
			{
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

	void MemoryManager::reportFallback(TId id, void* ptr, size_t requested)
	{
#if PROFILE_ENABLED
		if (unlikely(!IsValid(id)))
		{
			log(ELogLevel::Error, [funcName = __func__, id](auto& ls)
			{ ls << '[' << funcName << "] Invalid allocator id(" << id << ") is provided."; });

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

		log(ELogLevel::Verbose, [&stats, id, ptr, requested](auto& ls)
		{
			ls << "[FallbackAlloc][" << stats.name << "][" << id << "] "
			   << " PTR = " << ptr << ", Count = " << stats.fallbackCount << ", Requested = " << requested;
		});
#endif // PROFILE_ENABLED
	}

	void* MemoryManager::sysAllocate(size_t nBytes)
	{
		auto& allocator = allocators[SystemAllocatorID];
		if (unlikely(allocator.allocate == nullptr))
		{
			log(ELogLevel::FatalError, [funcName = __func__](auto& ls)
			{
				ls << '[' << funcName << "] "
				   << "SystemAllocator has no allocate function.";
			});

			return nullptr;
		}

		return allocator.allocate(&allocator, nBytes);
	}

	void MemoryManager::sysDeallocate(void* ptr, size_t nBytes)
	{
		auto& allocator = allocators[SystemAllocatorID];
		if (unlikely(allocator.deallocate == nullptr))
		{
			log(ELogLevel::FatalError, [funcName = __func__](auto& ls)
			{
				ls << '[' << funcName << "] "
				   << "SystemAllocator has no deallocate function.";
			});

			return;
		}

		allocator.deallocate(&allocator, ptr, nBytes);
	}
	void* MemoryManager::fallbackAllocate(TId id, TId parentId, size_t requestedSize)
	{
		auto ptr = allocate(parentId, requestedSize);

#if PROFILE_ENABLED
		reportFallback(id, ptr, requestedSize);
#endif // PROFILE_ENABLED

		return ptr;
	}

	void* MemoryManager::allocate(TId id, size_t nBytes)
	{
		if (unlikely(nBytes == 0))
		{
			return nullptr;
		}

		Assert(id != InvalidAllocatorID, "Memory allocation is not permitted.");

#if FORCE_USE_SYSTEM_MALLOC
		id = SystemAllocatorID;
#endif // __USE_SYSTEM_MALLOC__

		using namespace std;

		if (unlikely(!IsValid(id)))
		{
			log(ELogLevel::Error, [funcName = __func__, id](auto& ls)
			{
				ls << '[' << funcName << "] Invalid Scoped Allocator ID = " << id
				   << ", the default allocator shall be used.";
			});

			id = 0;
		}

		AllocatorProxy& allocProxy = allocators[id];
		Assert(allocProxy.allocate != nullptr, "[", getName(), "::", __func__, "][Error] ",
			   "No allocate function, ID = ", id);

		auto ptr = allocProxy.allocate(allocProxy.allocator, nBytes);
		Assert(ptr != nullptr, "Allocation Failed");

		return ptr;
	}

	void MemoryManager::Deallocate(TId id, void* ptr, size_t nBytes)
	{
		if (unlikely(ptr == nullptr))
		{
			Assert(nBytes == 0);
			return;
		}

		Assert(id != InvalidAllocatorID);

#if FORCE_USE_SYSTEM_MALLOC
		id = SystemAllocatorID;
#endif // __USE_SYSTEM_MALLOC__

		using namespace std;

		if (unlikely(!IsValid(id)))
		{
			log(ELogLevel::Error, [funcName = __func__, id](auto& ls)
			{
				ls << '[' << funcName << "] Invalid Scoped Allocator ID = " << id
				   << ", the default deallocate shall be used.";
			});

			id = 0;
		}

		auto& allocProxy = allocators[id];
		Assert(allocProxy.deallocate != nullptr, "[MemoryManager::Allocate][Error] No allocate function, ID = ", id);

		allocProxy.deallocate(allocProxy.allocator, ptr, nBytes);
	}

	void* MemoryManager::allocate(size_t nBytes) { return allocate(getScopedAllocatorID(), nBytes); }
	void MemoryManager::Deallocate(void* ptr, size_t nBytes) { Deallocate(getScopedAllocatorID(), ptr, nBytes); }

	bool MemoryManager::isLogEnabled(ELogLevel level) const
	{
#if MEMORY_LOGGING_ENABLED
		static TAtomicConfigParam<uint8_t> CPLogLevel("Log.Memory", "The Memory System Log Level",
													  static_cast<uint8_t>(Config::MemLogLevel));

		if (static_cast<uint8_t>(level) < CPLogLevel.get())
		{
			return false;
		}

		return true;
#else // MEMORY_LOGGING_ENABLED
		return false;
#endif // MEMORY_LOGGING_ENABLED
	}

	void MemoryManager::log(ELogLevel level, TLogFunc func) const
	{
#if MEMORY_LOGGING_ENABLED
		if (!isLogEnabled(level))
		{
			return;
		}

		auto& engine = Engine::get();
		engine.log(level, func);
#endif // MEMORY_LOGGING_ENABLED
	}

	const MultiPoolAllocatorConfig& MemoryManager::lookUpMultiPoolConfig(StaticStringID uniqueName) const
	{
		static const MultiPoolAllocatorConfig null;

		auto& data = multiPoolConfigCache.getData();
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
				log(ELogLevel::Warning, [&item](auto& ls)
				{
					StaticString name(item.uniqueName);
					for (auto& config : item.configs)
					{
						ls << "LookUpMultiPoolConfig: Found [" << name << "] (" << config.blockSize << ", "
						   << config.numberOfBlocks << ")\n";
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

	void MemoryManager::registerSystemAllocator()
	{
		using TSysAlloc = SystemAllocator<uint8_t>;
		static TSysAlloc systemAllocator;

		auto allocFunc = [](void*, size_t nBytes) -> void* { return systemAllocator.allocate(nBytes); };
		auto deallocFunc = [](void*, void* ptr, size_t nBytes)
		{ systemAllocator.deallocate(static_cast<uint8_t*>(ptr), nBytes); };

		auto& allocator = allocators[SystemAllocatorID];
		allocator.allocate = allocFunc;
		allocator.deallocate = deallocFunc;

		Assert(usage.totalCapacity == 0);
		Assert(usage.maxCapacity == 0);
		usage.totalCapacity = Config::MemCapacity;
		usage.maxCapacity = Config::MemCapacity;
		Assert(systemAllocator.getID() == SystemAllocatorID);
		setScopedAllocatorID(SystemAllocatorID);

#if PROFILE_ENABLED
		{
			std::lock_guard lockScope(statsLock);

			auto& stats = allocator.stats;

			stats.onRegister("SystemAllocator", false, Config::MemCapacity);
			auto& rec = usage;
			rec.totalCapacity += Config::MemCapacity;
			rec.maxCapacity = std::max(rec.maxCapacity, rec.totalCapacity);
		}
#endif // PROFILE_ENABLED

#if MEMORY_VERIFICATION_ENABLED
		allocator.threadId = std::this_thread::get_id();
#endif // MEMORY_VERIFICATION_ENABLED
	}

	void MemoryManager::deregisterSystemAllocator() { deregisterAllocator(SystemAllocatorID); }

	void MemoryManager::loadMultiPoolConfigs()
	{
		using namespace StringUtil;
		static const StaticString func(toCompactMethodName(__PRETTY_FUNCTION__));

		StaticString path(getMultiPoolConfigCacheFilePath());
		if (!OS::exist(path))
		{
			log(ELogLevel::Significant, [path](auto& ls) { ls << "Load MultiPoolConfig: Failed to find " << path; });

			return;
		}

		log(ELogLevel::Significant, [path](auto& ls) { ls << "Load MultiPoolConfig: " << path; });

		auto buffer = BufferUtil::getReadOnlyFileBuffer(path);
		if (buffer.getData() == nullptr)
		{
			log(ELogLevel::Error, [path](auto& ls) { ls << '[' << func << "] Failed to open the file " << path; });

			return;
		}

		if (!multiPoolConfigCache.deserialize(buffer))
		{
			log(ELogLevel::Error,
				[path](auto& ls) { ls << '[' << func << "] Failed to deserialize the file " << path; });

			return;
		}

#ifdef __DEBUG__
		constexpr auto logLevel = ELogLevel::Verbose;
		if (!isLogEnabled(logLevel))
		{
			return;
		}

		auto& data = multiPoolConfigCache.getData();
		size_t index = 0;

		for (auto& item : data)
		{
			StaticString itemName(item.uniqueName);
			auto& configs = item.configs;

			for (auto& config : configs)
			{
				log(logLevel, [&index, itemName, &config](auto& ls)
				{
					ls << index++ << " : " << itemName << " (" << config.blockSize << ", " << config.numberOfBlocks
					   << ')';
				});
			}
		}
#endif // __DEBUG__
	}

	void MemoryManager::saveMultiPoolConfigs()
	{
#if PROFILE_ENABLED
		using namespace StringUtil;
		static const StaticString func(toCompactMethodName(__PRETTY_FUNCTION__));

		auto GetOutputSize = [this]() -> size_t
		{
			auto buffer = BufferUtil::generateDummyBuffer();
			return multiPoolConfigLog.serialize(buffer);
		};

		const size_t size = GetOutputSize();
		auto pathStrID = getMultiPoolConfigCacheFilePath();
		StaticString path(pathStrID);

		log(ELogLevel::Info, [size, path](auto& ls)
		{ ls << '[' << func << "] MultiPoolConfig cache data(" << size << " bytes) shall be saved in " << path; });

		auto buffer = BufferUtil::getWriteOnlyFileBuffer(path, size);
		auto result = multiPoolConfigLog.serialize(buffer);

		if (result != size)
		{
			log(ELogLevel::Error,
				[path](auto& ls) { ls << '[' << func << "] Failed to deserialize the file " << path; });

			return;
		}

#ifdef __DEBUG__
		constexpr auto logLevel = ELogLevel::Verbose;
		if (!isLogEnabled(logLevel))
		{
			return;
		}

		auto& data = multiPoolConfigLog.getData();
		size_t index = 0;

		for (auto& item : data)
		{
			StaticString itemName(item.uniqueName);
			auto& configs = item.configs;

			for (auto& config : configs)
			{
				log(logLevel, [&index, itemName, &config](auto& ls)
				{
					ls << index++ << " : " << itemName << " (" << config.blockSize << ", " << config.numberOfBlocks
					   << ')';
				});
			}
		}
#endif // __DEBUG__
#endif // PROFILE_ENABLED
	}

	void MemoryManager::setScopedAllocatorID(TId id)
	{
		using namespace std;

		if (unlikely(id != InvalidAllocatorID && !IsValid(id)))
		{
			log(ELogLevel::Error, [funcName = __func__, id](auto& ls)
			{
				ls << '[' << funcName << "] Invalid Scoped Allocator ID = " << id
				   << ", the default deallocate shall be used.";
			});

			id = 0;
		}

		if (unlikely(dVarScopedAllocLogging))
		{
			log(ELogLevel::Error, [funcName = __func__, id](auto& ls)
			{ ls << '[' << funcName << "] Previous ID = " << id << ", New ID = " << id; });
		}

		scopedAllocatorID = id;
	}

} // namespace hbe
