// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <chrono>
#include <fstream>
#include <functional>
#include <thread>
#include "Core/Task.h"
#include "HSTL/HString.h"
#include "HSTL/HUnorderedMap.h"
#include "HSTL/HVector.h"
#include "LogLevel.h"
#include "LogLine.h"
#include "Memory/MultiPoolAllocator.h"
#include "Memory/ThreadSafeMultiPoolAllocator.h"
#include "String/InlineStringBuilder.h"
#include "String/StaticString.h"


namespace hbe
{

class Engine;
class TaskSystem;

/// @brief Asynchronous, thread-safe logging system for the HardBop Engine.
/// @details Runs on a dedicated IO thread to avoid blocking main application threads.
/// Uses a producer-consumer pattern with thread-safe input buffer.
class Logger final
{
public:
	using TString = hbe::HString;
	using TLogBuffer = hbe::HVector<LogLine>;
	using TTextBuffer = hbe::HVector<TString>;
	using TTimePoint = std::chrono::time_point<std::chrono::steady_clock>;
	using TLogStream = InlineStringBuilder<Config::LogOutputBuffer>;
	using TLogFunction = std::function<void(TLogStream&)>;
	using TOutputFunc = std::function<void(const TTextBuffer&)>;
	using TOutputFuncs = hbe::HVector<TOutputFunc>;
	using TLogFilter = std::function<bool(ELogLevel)>;
	using TFilters = hbe::HUnorderedMap<StaticString, TLogFilter>;

public:
	/// @brief A lightweight logger wrapper that bundles category and default log level.
	/// @details Simplifies logging by pre-setting category and level. Can be reused
	/// across multiple log calls with the same settings.
	class SimpleLogger final
	{
	public:
		const StaticString category;
		const ELogLevel level;

		explicit SimpleLogger(StaticString category, ELogLevel level = ELogLevel::Info) noexcept;
		void out(const TLogFunction& logFunc) const noexcept;
		void out(ELogLevel level, const TLogFunction& logFunc) const noexcept;

		void outWarning(const TLogFunction& logFunc) const noexcept { out(ELogLevel::Warning, logFunc); }
		void outError(const TLogFunction& logFunc) const noexcept { out(ELogLevel::Error, logFunc); }
		void outFatalError(const TLogFunction& logFunc) const noexcept { out(ELogLevel::FatalError, logFunc); }

		void out(const char* text) const noexcept
		{
			out([text](auto& ls) { ls << text; });
		}

		void out(ELogLevel level, const char* text) const noexcept
		{
			out(level, [text](auto& ls) { ls << text; });
		}

		void outWarning(const char* text) const noexcept
		{
			outWarning([text](auto& ls) { ls << text; });
		}

		void outError(const char* text) const noexcept
		{
			outError([text](auto& ls) { ls << text; });
		}

		void outFatalError(const char* text) const noexcept
		{
			outFatalError([text](auto& ls) { ls << text; });
		}
	};

private:
	static Logger* instance;
	MultiPoolAllocator allocator;
	ThreadSafeMultiPoolAllocator inputAlloc;
	Task task;
	std::atomic<bool> isRunning;
	std::atomic<bool> hasInput;
	std::atomic<bool> needFlush;

	TString logPath;
	TLogBuffer inputBuffer;
	TLogBuffer swapBuffer;
	TTextBuffer textBuffer;
	TOutputFuncs flushFuncs;
	TFilters filters;

	std::ofstream outFileStream;
	std::thread::id threadID;

	std::mutex filterLock;
	std::mutex inputLock;

public:
	static Logger& get() noexcept;
	static SimpleLogger get(StaticString category, ELogLevel level = ELogLevel::Info) noexcept;

public:
	Logger(const Logger&) = delete;
	Logger(Logger&&) = delete;
	Logger& operator=(const Logger&) = delete;
	Logger& operator=(Logger&&) = delete;

public:
	Logger(Engine& engine, const char* path, const char* filename) noexcept;
	~Logger() noexcept;

	[[nodiscard]] static StaticString getName() noexcept;
	void startTask(TaskSystem& taskSys);
	void stopTask(TaskSystem& taskSys);

	void setFilter(StaticString category, TLogFilter&& filter) noexcept;
	void addLog(StaticString category, ELogLevel level, const TLogFunction& logFunc) noexcept;
	void flush() noexcept;

#if PROFILE_ENABLED
	void reportMemoryConfiguration();
#endif // PROFILE_ENABLED

private:
	void processBuffer() noexcept;
	void flushBuffer(const TTextBuffer& buffer) const noexcept;

	void writeLog(const TTextBuffer& buffer) noexcept;
	static void printStdIO(const TTextBuffer& buffer) noexcept;
};

using TLog = Logger::SimpleLogger;
using LogStream = Logger::TLogStream;

} // namespace hbe
