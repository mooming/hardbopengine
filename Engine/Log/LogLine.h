// Created by mooming.go@gmail.com

#pragma once

#include <chrono>
#include "Config/EngineConfig.h"
#include "LogLevel.h"
#include "String/StaticString.h"

namespace hbe
{

	/// @brief Represents a single log entry with metadata and message text.
	/// Uses a union to optimize memory: short messages stored inline, long messages use dynamic allocation.
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

} // namespace hbe
