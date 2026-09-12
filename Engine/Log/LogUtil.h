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

	const TTimePoint& GetStartTime() noexcept;
	void GetTimeStampString(InlineStringBuilder<64>& outStr,
							const TTimePoint& currentTime = std::chrono::steady_clock::now()) noexcept;
	[[nodiscard]] StaticString GetLogLevelString(ELogLevel level) noexcept;

} // namespace LogUtil

} // namespace hbe
