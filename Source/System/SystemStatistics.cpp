// Created by mooming.go@gmail.com, 2022

#include "SystemStatistics.h"

#include "Log/Logger.h"
#include "OSAL/SourceLocation.h"
#include "String/StaticString.h"


namespace HE
{

#ifdef PROFILE_ENABLED
SystemStatistics::AllocProfile::AllocProfile()
    : allocatorName("")
    , file(nullptr)
    , func(nullptr)
    , lineNumber(0)
    , columnNumber(0)
    , maxUsage(0)
{
}

SystemStatistics::AllocProfile::AllocProfile(const char* inName
    , const char* file, const char* func
    , size_t lineNumber
    , size_t columnNumber, size_t maxUsage)

    : file(file)
    , func(func)
    , lineNumber(lineNumber)
    , columnNumber(columnNumber)
    , maxUsage(maxUsage)
{
    constexpr auto LastIndex = AllocatorProxy::NameBufferSize - 1;
    strncpy(allocatorName, inName, LastIndex);
    allocatorName[LastIndex] = '\0';
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
