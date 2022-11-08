// Created by mooming.go@gmail.com, 2017

#pragma once

#include "LogLevel.h"
#include "HSTL/HString.h"
#include "String/StaticString.h"
#include <chrono>


namespace HE
{

namespace LogUtil
{
using TTimePoint = std::chrono::time_point<std::chrono::steady_clock>;

const TTimePoint& GetStartTime();
HSTL::HInlineString<64> GetTimeStampString(const TTimePoint& currentTime = std::chrono::steady_clock::now());
StaticString GetLogLevelString(ELogLevel level);

} // LogUtil

} // HE
