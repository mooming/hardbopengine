// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <chrono>
#include "Config/EngineConfig.h"
#include "LogLevel.h"
#include "String/StaticString.h"

namespace hbe
{

/// @brief Represents a single log entry with metadata and message text.
/// Uses a union to optimize memory: short messages stored inline, long messages use dynamic allocation.
class LogLine final
{
public:
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

	LogLine() noexcept;
	LogLine(LogLine&& rhs) noexcept;
	LogLine(ELogLevel level, StaticString threadName, StaticString category, const char* inText, size_t size) noexcept;
	~LogLine() noexcept;

	[[nodiscard]] const char* GetText() const noexcept;
};

} // namespace hbe
