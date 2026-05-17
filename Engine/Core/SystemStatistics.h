// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

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

/// @brief Tracks engine runtime statistics including frame counts, timing, and memory usage.
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

	[[nodiscard]] const StaticString& GetName() const noexcept;
	void UpdateCurrentTime() noexcept;

#if PROFILE_ENABLED
	void Report(const AllocStats& allocStats) noexcept;
	void ReportSysMemAlloc(size_t usage) noexcept;
	void ReportSysMemDealloc(size_t usage) noexcept;
#endif // PROFILE_ENABLED

	void Print() noexcept;
	void PrintAllocatorProfiles() noexcept;

	void IncFrameCount() noexcept { frameCount.fetch_add(1, std::memory_order_relaxed); }
	void IncSlowFrameCount() noexcept { slowFrameCount.fetch_add(1, std::memory_order_relaxed); }
	void IncEngineLogCount() noexcept { engineLogCount.fetch_add(1, std::memory_order_relaxed); }
	void IncLogCount() noexcept { logCount.fetch_add(1, std::memory_order_relaxed); }
	void IncLongLogCount() noexcept { longLogCount.fetch_add(1, std::memory_order_relaxed); }
	void IncFallbackAllocCount() noexcept { fallbackAllocCount.fetch_add(1, std::memory_order_relaxed); }

	[[nodiscard]] auto GetStartTime() const noexcept { return startTime; }
	[[nodiscard]] auto GetCurrentTime() const noexcept { return currentTime; }
	[[nodiscard]] auto GetTimeSinceStart() const noexcept { return timeSinceStart; }
	[[nodiscard]] auto GetDeltaTime() const noexcept { return deltaTime; }
};

} // namespace hbe
