// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <chrono>
#include "Types.h"


namespace hbe::time
{
using TMilliSec = uint64_t;
using TStopWatch = std::chrono::steady_clock;
using TTime = std::chrono::time_point<TStopWatch>;
using TDuration = TStopWatch::duration;
using TMilliSecs = std::chrono::milliseconds;

[[nodiscard]] inline TTime GetNow() noexcept { return TStopWatch::now(); }

[[nodiscard]] inline float ToFloat(TStopWatch::duration duration) noexcept
{
std::chrono::duration<float> delta = duration;
return delta.count();
}

[[nodiscard]] inline double ToDouble(TStopWatch::duration duration) noexcept
{
std::chrono::duration<double> delta = duration;
return delta.count();
}

[[nodiscard]] inline TMilliSecs::rep ToMilliSeconds(TStopWatch::duration duration) noexcept
{
auto delta = std::chrono::duration_cast<TMilliSecs>(duration);
return delta.count();
}

void Sleep(TMilliSec milli) noexcept;
} // namespace Time

