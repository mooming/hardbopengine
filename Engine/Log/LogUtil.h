// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <chrono>
#include "LogLevel.h"
#include "String/InlineStringBuilder.h"
#include "String/StaticString.h"

namespace hbe
{

/// @brief Utility functions for logging operations
namespace LogUtil
{

using TTimePoint = std::chrono::time_point<std::chrono::steady_clock>;

/// @brief The instant timestamps are measured against, so a log reads as time-since-start.
/// @note Defaults to the first timestamped line. Engine::Engine overrides that with its own
///       construction, so a normal process is relative to engine start rather than to the moment
///       logging happened to begin. Left alone in a process that never builds an Engine, the
///       numbers stay meaningful instead of being a raw clock value.
const TTimePoint& GetStartTime() noexcept;

/// @brief Make the current instant the zero point returned by GetStartTime.
/// @note Call it once, where the process decides it has started, before any line is logged. Moving
///       the zero point afterwards shifts every later line relative to the ones already written,
///       which is why Engine sets it in its constructor and not in Initialize. It reads the log
///       clock itself rather than taking an instant as an argument: SystemStatistics keeps its start
///       in time::TTime, over high_resolution_clock, and that clock is system_clock on some standard
///       libraries - converting across the two would be a unit error waiting for a toolchain change.
void ResetStartTime() noexcept;
void GetTimeStampString(InlineStringBuilder<64>& outStr,
						const TTimePoint& currentTime = std::chrono::steady_clock::now()) noexcept;
[[nodiscard]] StaticString GetLogLevelString(ELogLevel level) noexcept;

} // namespace LogUtil

} // namespace hbe
