// Created by mooming.go@gmail.com, 2022

#pragma once

#include <atomic>
#include <mutex>
#include <vector>
#include "Config/BuildConfig.h"
#include "Memory/AllocStats.h"
#include "Time.h"

namespace hbe
{

	class Engine;
	class StaticString;

	class SystemStatistics final
	{
	private:
		static_assert(std::atomic<uint64_t>::is_always_lock_free);
		std::atomic<uint64_t> frameCount;
		std::atomic<uint64_t> slowFrameCount;
		std::atomic<uint64_t> engineLogCount;
		std::atomic<uint64_t> logCount;
		std::atomic<uint64_t> longLogCount;
		std::atomic<uint64_t> fallbackAllocCount;

		std::mutex sysMemReportLock;
		size_t allocCount;
		size_t deallocCount;
		size_t totalUsage;
		size_t maxUsage;

		time::TTime startTime;
		time::TTime currentTime;
		double timeSinceStart;
		float deltaTime;

#if PROFILE_ENABLED
		std::vector<AllocStats> allocStats;
#endif // PROFILE_ENABLED

	public:
		explicit SystemStatistics(Engine& engine);
		~SystemStatistics() = default;

		const StaticString& GetName() const;
		void UpdateCurrentTime();

#if PROFILE_ENABLED
		void Report(const AllocStats& allocStats);
		void ReportSysMemAlloc(size_t usage);
		void ReportSysMemDealloc(size_t usage);
#endif // PROFILE_ENABLED

		void Print();
		void PrintAllocatorProfiles();

		inline void IncFrameCount() { frameCount.fetch_add(1, std::memory_order_relaxed); }
		inline void IncSlowFrameCount() { slowFrameCount.fetch_add(1, std::memory_order_relaxed); }
		inline void IncEngineLogCount() { engineLogCount.fetch_add(1, std::memory_order_relaxed); }
		inline void IncLogCount() { logCount.fetch_add(1, std::memory_order_relaxed); }
		inline void IncLongLogCount() { longLogCount.fetch_add(1, std::memory_order_relaxed); }
		inline void IncFallbackAllocCount() { fallbackAllocCount.fetch_add(1, std::memory_order_relaxed); }

		inline auto GetStartTime() const { return startTime; }
		inline auto GetCurrentTime() const { return currentTime; }
		inline auto GetTimeSinceStart() const { return timeSinceStart; }
		inline auto GetDeltaTime() const { return deltaTime; }
	};

} // namespace hbe
