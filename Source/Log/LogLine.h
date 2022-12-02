// Created by mooming.go@gmail.com 2022

#pragma once

#include "LogLevel.h"
#include "Config/EngineSettings.h"
#include "String/StaticString.h"
#include <chrono>

namespace HE
{

struct LogLine final
{
    using TTimePoint = typename std::chrono::time_point<std::chrono::steady_clock>;
    
    TTimePoint timeStamp;
    StaticString threadName;
    StaticString category;
    ELogLevel level;
    bool isLong;

    union
    {
        char text[Config::LogLineLength];
        struct
        {
            char* longText;
            size_t longTextSize;
        };
    };

    LogLine();
    LogLine(LogLine&& rhs);
    LogLine(ELogLevel level, StaticString threadName, StaticString category, const char* inText, size_t size);
    ~LogLine();

    const char* GetText() const;
};

} // HE
