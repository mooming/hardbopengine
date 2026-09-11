// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <chrono>
#include "Types.h"


namespace hbe::time
{
using TMilliSec = uint64_t;
using TStopWatch = std::chrono::high_resolution_clock;
using TTime = std::chrono::time_point<TStopWatch>;
using TDuration = TStopWatch::duration;
using TMilliSecs = std::chrono::milliseconds;

[[nodiscard]] inline TTime getNow() noexcept { return std::chrono::steady_clock::now(); }

[[nodiscard]] inline float toFloat(TStopWatch::duration duration) noexcept
{
std::chrono::duration<float> delta = duration;
return delta.count();
}

[[nodiscard]] inline double toDouble(TStopWatch::duration duration) noexcept
{
std::chrono::duration<double> delta = duration;
return delta.count();
}

[[nodiscard]] inline TMilliSecs::rep toMilliSeconds(TStopWatch::duration duration) noexcept
{
auto delta = std::chrono::duration_cast<TMilliSecs>(duration);
return delta.count();
}

void Sleep(TMilliSec milli) noexcept;
} // namespace Time

