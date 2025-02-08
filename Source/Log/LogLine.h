// Created by mooming.go@gmail.com

#pragma once

#include "Config/EngineSettings.h"
#include "LogLevel.h"
#include "String/StaticString.h"
#include <chrono>

namespace HE
{

    struct LogLine final
    {
        using TTimePoint =
            typename std::chrono::time_point<std::chrono::steady_clock>;

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
        LogLine(ELogLevel level, StaticString threadName, StaticString category,
            const char* inText, size_t size);
        ~LogLine();

        const char* GetText() const;
    };

} // namespace HE
