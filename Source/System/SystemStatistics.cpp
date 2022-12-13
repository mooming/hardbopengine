// Created by mooming.go@gmail.com, 2022

#include "SystemStatistics.h"

#include "Engine.h"
#include "Log/Logger.h"
#include "OSAL/OSDebug.h"
#include "String/StaticString.h"
#include "String/StringUtil.h"


namespace HE
{

SystemStatistics::SystemStatistics(Engine& engine)
    : frameCount(0)
    , slowFrameCount(0)
    , engineLogCount(0)
    , logCount(0)
    , longLogCount(0)
    , fallbackAllocCount(0)
    , allocCount(0)
    , deallocCount(0)
    , totalUsage(0)
    , maxUsage(0)
    , startTime(Time::TStopWatch::now())
    , currentTime(startTime)
{
    Assert(engine.IsMemoryManagerReady());
    engine.SetSystemStatisticsReady();
}

const StaticString& SystemStatistics::GetName() const
{
    static StaticString name("SystemStatistics");
    return name;
}

void SystemStatistics::UpdateCurrentTime()
{
    {
        auto newTime = Time::TStopWatch::now();
        const auto delta = newTime - currentTime;
        deltaTime = Time::ToFloat(delta);
        currentTime = newTime;
    }

    const auto fromStart = currentTime - startTime;
    timeSinceStart = Time::ToDouble(fromStart);
}

#ifdef PROFILE_ENABLED
void SystemStatistics::Report(const AllocStats& stats)
{
    allocStats.emplace_back(stats);
}

void SystemStatistics::ReportSysMemAlloc(size_t usage)
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
        static auto log = Logger::Get(ToCompactMethodName(__PRETTY_FUNCTION__));

        log.OutWarning([localTotalUsage](auto& ls)
        {
            ls << "System Memory Usage " << localTotalUsage
                << " exceeds its limit " << Config::MemCapacity;
        });
    }
}

void SystemStatistics::ReportSysMemDealloc(size_t usage)
{
    std::lock_guard lock(sysMemReportLock);
    ++deallocCount;

    Assert(totalUsage >= usage);
    totalUsage -= usage;
}
#endif // PROFILE_ENABLED

void SystemStatistics::Print()
{
    auto curLogCount = logCount.load(std::memory_order_relaxed);
    auto curLongLogCount = longLogCount.load(std::memory_order_relaxed);

    auto log = Logger::Get(GetName());

    log.Out("= System Statistics ==========================");
    log.Out([this](auto& ls)
    {
        ls << "Frame Count = " << frameCount.load(std::memory_order_relaxed);
    });

    log.Out([this](auto& ls)
    {
        ls << "Slow Frame Count = " << slowFrameCount.load(std::memory_order_relaxed);
    });

    log.Out([this](auto& ls)
    {
        ls << "Engine Log Count = " << engineLogCount.load(std::memory_order_relaxed);
    });

    log.Out([curLogCount](auto& ls)
    {
        ls << "Log Count = " << curLogCount;
    });

    log.Out([curLongLogCount, curLogCount](auto& ls)
    {
        ls << "Long Log Count = " << curLongLogCount
            << " / " << curLogCount;
    });

    log.Out([this](auto& ls)
    {
        ls << "Running Time = " << timeSinceStart << " sec";
    });

    log.Out([this](auto& ls)
    {
        ls << "Allocation Count = " << allocCount << " (Alloc), "
            << deallocCount << " (Dealloc)";
    });

    log.Out([this](auto& ls)
    {
        constexpr size_t MegaBytes = 1024UL * 1024;
        ls << "System Memory Usage = " << totalUsage << " ("
            << (totalUsage / MegaBytes) << " MB) / " << maxUsage
            << " (" << (maxUsage / MegaBytes) << " MB)";
    });

    log.Out([this](auto& ls)
    {
        ls << "Fallback Allocation Count = "
            << fallbackAllocCount.load(std::memory_order_relaxed);
    });

    log.Out("==============================================");
}

void SystemStatistics::PrintAllocatorProfiles()
{
#ifdef PROFILE_ENABLED
    auto log = Logger::Get(GetName(), ELogLevel::Verbose);
    log.Out("= System Statistics: Allocator Profiles ========================");

    for (auto& stats : allocStats)
    {
        stats.Print();
    }

    log.Out("================================================================");
#endif //PROFILE_ENABLED
}

} // HE
