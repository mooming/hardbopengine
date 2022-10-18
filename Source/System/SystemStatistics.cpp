// Created by mooming.go@gmail.com, 2022

#include "SystemStatistics.h"

#include "Log/Logger.h"
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

void SystemStatistics::Print()
{
    auto log = Logger::Get(GetName());
    log.Out("= System Statistics ==========================");
    log.Out([this](auto& ls)
    {
        ls << "Frame Count = " << frameCount.load();
    });

    log.Out([this](auto& ls)
    {
        ls << "Slow Frame Count = " << slowFrameCount.load();
    });

    log.Out([this](auto& ls)
    {
        ls << "Running Time = " << timeSinceStart;
    });

    log.Out("==============================================");
}

} // HE
