// Created by mooming.go@gmail.com, 2022

#include "SystemStatistics.h"

#include "Log/Logger.h"
#include "OSAL/OSDebug.h"
#include "String/StaticString.h"


namespace HE
{

SystemStatistics::SystemStatistics()
    : frameCount(0)
    , slowFrameCount(0)
    , startTime(Time::TStopWatch::now())
    , currentTime(startTime)
{
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

    log.Out("==============================================");
}

void SystemStatistics::PrintAllocatorProfiles()
{
#ifdef PROFILE_ENABLED
    auto log = Logger::Get(GetName());
    log.Out("= System Statistics: Allocator Profiles ========================");

    for (auto& stats : allocStats)
    {
        stats.Print();
    }

    log.Out("================================================================");
#endif //PROFILE_ENABLED
}

} // HE
