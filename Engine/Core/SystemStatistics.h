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

	[[nodiscard]] const StaticString& getName() const noexcept;
	void updateCurrentTime() noexcept;

#if PROFILE_ENABLED
	void report(const AllocStats& allocStats) noexcept;
	void reportSysMemAlloc(size_t usage) noexcept;
	void reportSysMemDealloc(size_t usage) noexcept;
#endif // PROFILE_ENABLED

	void print() noexcept;
	void printAllocatorProfiles() noexcept;

	void incFrameCount() noexcept { frameCount.fetch_add(1, std::memory_order_relaxed); }
	void incSlowFrameCount() noexcept { slowFrameCount.fetch_add(1, std::memory_order_relaxed); }
	void incEngineLogCount() noexcept { engineLogCount.fetch_add(1, std::memory_order_relaxed); }
	void incLogCount() noexcept { logCount.fetch_add(1, std::memory_order_relaxed); }
	void incLongLogCount() noexcept { longLogCount.fetch_add(1, std::memory_order_relaxed); }
	void incFallbackAllocCount() noexcept { fallbackAllocCount.fetch_add(1, std::memory_order_relaxed); }

	[[nodiscard]] auto getStartTime() const noexcept { return startTime; }
	[[nodiscard]] auto getCurrentTime() const noexcept { return currentTime; }
	[[nodiscard]] auto getTimeSinceStart() const noexcept { return timeSinceStart; }
	[[nodiscard]] auto getDeltaTime() const noexcept { return deltaTime; }
};

} // namespace hbe
