// Created by mooming.go@gmail.com 2022

#pragma once

#include "LogLevel.h"
#include "HSTL/HString.h"
#include "String/StaticString.h"
#include <chrono>
#include <memory>


namespace HE
{

struct LogLine final
{
    using TString = HSTL::HString;
    using TTimePoint = std::chrono::time_point<std::chrono::steady_clock>;
    
    TTimePoint timeStamp;
    StaticString category;
    ELogLevel level;
    TString text;
    
    inline LogLine()
        : level(ELogLevel::Info)
    {
    }
    
    inline LogLine(ELogLevel level, StaticString category, TString&& text)
        : timeStamp(std::chrono::steady_clock::now())
        , category(category)
        , level(level)
        , text(std::move(text))
    {
    }
};

} // HE
