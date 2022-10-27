// Created by mooming.go@gmail.com, 2022

#include "SystemStatistics.h"

#include "Log/Logger.h"
#include "OSAL/SourceLocation.h"
#include "String/StaticString.h"


namespace HE
{

#ifdef PROFILE_ENABLED
SystemStatistics::AllocProfile::AllocProfile()
    : allocatorName(nullptr)
    , file(nullptr)
    , func(nullptr)
    , lineNumber(0)
    , columnNumber(0)
    , maxUsage(0)
{
}

SystemStatistics::AllocProfile::AllocProfile(const char* allocatorName
    , const char* file, const char* func
    , size_t lineNumber
    , size_t columnNumber, size_t maxUsage)

    : allocatorName(allocatorName)
    , file(file)
    , func(func)
    , lineNumber(lineNumber)
    , columnNumber(columnNumber)
    , maxUsage(maxUsage)
{
}
#endif // PROFILE_ENABLED

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
void SystemStatistics::Report(const char* allocatorName
    , const std::source_location& location, size_t maxUsage)
{
    allocatorProfiles.emplace_back(allocatorName, location.file_name()
       , location.function_name()
       , location.line(), location.column(), maxUsage);
}
#endif // PROFILE_ENABLED

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

void SystemStatistics::PrintAllocatorProfiles()
{
#ifdef PROFILE_ENABLED
    auto log = Logger::Get(GetName());
    log.Out("= System Statistics: Allocator Profiles ========================");

    for (auto& item : allocatorProfiles)
    {
        log.Out([item](auto& ls)
        {
            ls << item.allocatorName << ':'  << item.file
                << ':' << item.lineNumber
                << ':' << item.columnNumber << '='
                << item.maxUsage;
        });
    }

    log.Out("================================================================");
#endif //PROFILE_ENABLED
}

} // HE
