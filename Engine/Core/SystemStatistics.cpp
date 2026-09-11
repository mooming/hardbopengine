// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "SystemStatistics.h"

#include "../Engine/Engine.h"
#include "Log/Logger.h"
#include "OSAL/OSDebug.h"
#include "String/StaticString.h"
#include "String/StringUtil.h"


namespace hbe
{

SystemStatistics::SystemStatistics(Engine& engine) :
	frameCount(0), slowFrameCount(0), engineLogCount(0), logCount(0), longLogCount(0), fallbackAllocCount(0),
	allocCount(0), deallocCount(0), totalUsage(0), maxUsage(0), startTime(time::TStopWatch::now()),
	currentTime(startTime)
{
	Assert(engine.IsMemoryManagerReady());
	engine.setSystemStatisticsReady();
}

const StaticString& SystemStatistics::getName() const noexcept
{
	static StaticString name("SystemStatistics");
	return name;
}

void SystemStatistics::updateCurrentTime() noexcept
{
	{
		auto newTime = time::TStopWatch::now();
		const auto delta = newTime - currentTime;
		deltaTime = time::toFloat(delta);
		currentTime = newTime;
	}

	const auto fromStart = currentTime - startTime;
	timeSinceStart = time::toDouble(fromStart);
}

#if PROFILE_ENABLED
void SystemStatistics::report(const AllocStats& stats) { allocStats.emplace_back(stats); }

void SystemStatistics::reportSysMemAlloc(size_t usage)
{
	size_t localTotalUsage = 0;

	{
		std::lock_guard lock(sysMemReportLock);
		++allocCount;
		totalUsage += usage;
		maxUsage = std::max(maxUsage, totalUsage);
		localTotalUsage = totalUsage;
	}

	if (unlikely(localTotalUsage > Config::MemCapacity))
	{
		using namespace StringUtil;
		static auto log = Logger::get(toCompactMethodName(__PRETTY_FUNCTION__));

		log.outWarning([localTotalUsage](auto& ls)
		{ ls << "System Memory Usage " << localTotalUsage << " exceeds its limit " << Config::MemCapacity; });
	}
}

void SystemStatistics::reportSysMemDealloc(size_t usage)
{
	std::lock_guard lock(sysMemReportLock);
	++deallocCount;

	Assert(totalUsage >= usage);
	totalUsage -= usage;
}
#endif // PROFILE_ENABLED

void SystemStatistics::print() noexcept
{
	auto curLogCount = logCount.load(std::memory_order_relaxed);
	auto curLongLogCount = longLogCount.load(std::memory_order_relaxed);

	auto log = Logger::get(getName());

	log.out("= System Statistics ==========================");
	log.out([this](auto& ls) { ls << "Frame Count = " << frameCount.load(std::memory_order_relaxed); });

	log.out([this](auto& ls) { ls << "Slow Frame Count = " << slowFrameCount.load(std::memory_order_relaxed); });

	log.out([this](auto& ls) { ls << "Engine Log Count = " << engineLogCount.load(std::memory_order_relaxed); });

	log.out([curLogCount](auto& ls) { ls << "Log Count = " << curLogCount; });

	log.out([curLongLogCount, curLogCount](auto& ls)
	{ ls << "Long Log Count = " << curLongLogCount << " / " << curLogCount; });

	log.out([this](auto& ls) { ls << "Running Time = " << timeSinceStart << " sec"; });

	log.out([this](auto& ls)
	{ ls << "Allocation Count = " << allocCount << " (Alloc), " << deallocCount << " (Dealloc)"; });

	log.out([this](auto& ls)
	{
		constexpr size_t MegaBytes = 1024UL * 1024;
		ls << "System Memory Usage = " << totalUsage << " (" << (totalUsage / MegaBytes) << " MB) / " << maxUsage
		   << " (" << (maxUsage / MegaBytes) << " MB)";
	});

	log.out([this](auto& ls)
	{ ls << "Fallback Allocation Count = " << fallbackAllocCount.load(std::memory_order_relaxed); });

	log.out("==============================================");
}

void SystemStatistics::printAllocatorProfiles() noexcept
{
#if PROFILE_ENABLED
	auto log = Logger::get(getName(), ELogLevel::Verbose);
	log.out("= System Statistics: Allocator Profiles ========================");

	for (auto& stats : allocStats)
	{
		stats.print();
	}

	log.out("================================================================");
#endif // PROFILE_ENABLED
}

} // namespace hbe
