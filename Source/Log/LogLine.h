// Created by mooming.go@gmail.com 2022

#pragma once

#include "LogLevel.h"
#include "Config/EngineConfig.h"
#include "HSTL/HString.h"
#include "String/StaticString.h"
#include <chrono>
#include <memory>


namespace HE
{

struct LogLine final
{
    using TTimePoint = std::chrono::time_point<std::chrono::steady_clock>;
    
    TTimePoint timeStamp;
    StaticString category;
    ELogLevel level;
    char text[Config::LogLineSize];
    
    inline LogLine()
        : level(ELogLevel::Info)
    {
    }
    
    inline LogLine(ELogLevel level, StaticString category, const char* inText)
        : timeStamp(std::chrono::steady_clock::now())
        , category(category)
        , level(level)
    {
        constexpr auto LastIndex = Config::LogLineSize - 1;
        strncpy(text, inText, LastIndex);
        text[LastIndex] = '\0';
    }
};

} // HE
