// Created by mooming.go@gmail.com 2022

#pragma once

#include "LogLevel.h"
#include "Config/EngineSettings.h"
#include "String/StaticString.h"


namespace HE
{

struct LogLine final
{
    using TTimePoint = std::chrono::time_point<std::chrono::steady_clock>;
    
    TTimePoint timeStamp;
    StaticString threadName;
    StaticString category;
    ELogLevel level;
    bool isLong;

    union
    {
        char text[Config::LogLineLength];
        char* longText;
    };

    LogLine();
    LogLine(LogLine&& rhs);
    LogLine(ELogLevel level, StaticString threadName, StaticString category, const char* inText);
    LogLine(ELogLevel level, StaticString threadName, StaticString category, bool isLong, const char* inText);
    ~LogLine();

    const char* GetText() const;
};

} // HE
