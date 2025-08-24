// Created by mooming.go@gmail.com

#pragma once

#include <chrono>
#include <fstream>
#include <functional>
#include <thread>
#include "HSTL/HString.h"
#include "HSTL/HUnorderedMap.h"
#include "HSTL/HVector.h"
#include "LogLevel.h"
#include "LogLine.h"
#include "Memory/MultiPoolAllocator.h"
#include "Memory/ThreadSafeMultiPoolAllocator.h"
#include "String/InlineStringBuilder.h"
#include "String/StaticString.h"
#include "System/TaskHandle.h"

namespace hbe
{

	class Engine;
	class TaskSystem;

	class Logger final
	{
	public:
		using TString = HSTL::HString;
		using TLogBuffer = HSTL::HVector<LogLine>;
		using TTextBuffer = HSTL::HVector<TString>;
		using TTimePoint = std::chrono::time_point<std::chrono::steady_clock>;
		using TLogStream = InlineStringBuilder<Config::LogOutputBuffer>;
		using TLogFunction = std::function<void(TLogStream&)>;
		using TOutputFunc = std::function<void(const TTextBuffer&)>;
		using TOutputFuncs = HSTL::HVector<TOutputFunc>;
		using TLogFilter = std::function<bool(ELogLevel)>;
		using TFilters = HSTL::HUnorderedMap<StaticString, TLogFilter>;

	public:
		struct SimpleLogger final
		{
			const StaticString category;
			const ELogLevel level;

			explicit SimpleLogger(StaticString category, ELogLevel level = ELogLevel::Info);
			void Out(const TLogFunction& logFunc) const;
			void Out(ELogLevel level, const TLogFunction& logFunc) const;

			void OutWarning(const TLogFunction& logFunc) const { Out(ELogLevel::Warning, logFunc); }
			void OutError(const TLogFunction& logFunc) const { Out(ELogLevel::Error, logFunc); }
			void OutFatalError(const TLogFunction& logFunc) const { Out(ELogLevel::FatalError, logFunc); }

			void Out(const char* text) const
			{
				Out([text](auto& ls) { ls << text; });
			}

			void Out(ELogLevel level, const char* text) const
			{
				Out(level, [text](auto& ls) { ls << text; });
			}

			void OutWarning(const char* text) const
			{
				OutWarning([text](auto& ls) { ls << text; });
			}

			void OutError(const char* text) const
			{
				OutError([text](auto& ls) { ls << text; });
			}

			void OutFatalError(const char* text) const
			{
				OutFatalError([text](auto& ls) { ls << text; });
			}
		};

	private:
		static Logger* instance;

		MultiPoolAllocator allocator;
		ThreadSafeMultiPoolAllocator inputAlloc;
		TaskHandle taskHandle;
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
		static Logger& Get();
		static SimpleLogger Get(StaticString category, ELogLevel level = ELogLevel::Info);

	public:
		Logger(const Logger&) = delete;
		Logger(Logger&&) = delete;
		Logger& operator=(const Logger&) = delete;
		Logger& operator=(Logger&&) = delete;

	public:
		Logger(Engine& engine, const char* path, const char* filename);
		~Logger();

		static StaticString GetName();
		void StartTask(TaskSystem& taskSys);
		void StopTask(TaskSystem& taskSys);

		void SetFilter(StaticString category, TLogFilter&& filter);
		void AddLog(StaticString category, ELogLevel level, const TLogFunction& logFunc);
		void Flush();

#if PROFILE_ENABLED
		void ReportMemoryConfiguration();
#endif // PROFILE_ENABLED

	private:
		void ProcessBuffer();
		void FlushBuffer(const TTextBuffer& buffer);

		void WriteLog(const TTextBuffer& buffer);
		static void PrintStdIO(const TTextBuffer& buffer);
	};

	using TLog = Logger::SimpleLogger;
	using LogStream = Logger::TLogStream;

} // namespace hbe
