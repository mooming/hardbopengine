// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <chrono>
#include <fstream>
#include <functional>
#include <thread>

#include "Core/TaskID.h"
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

class TaskStream;

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
		void Out(const TLogFunction& logFunc) const noexcept;
		void Out(ELogLevel level, const TLogFunction& logFunc) const noexcept;

		void OutWarning(const TLogFunction& logFunc) const noexcept
		{
			Out(ELogLevel::Warning, logFunc);
		}

		void OutError(const TLogFunction& logFunc) const noexcept
		{
			Out(ELogLevel::Error, logFunc);
		}

		void OutFatalError(const TLogFunction& logFunc) const noexcept
		{
			Out(ELogLevel::FatalError, logFunc);
		}

		void Out(const char* text) const noexcept
		{
			Out([text](auto& ls) { ls << text; });
		}

		void Out(ELogLevel level, const char* text) const noexcept
		{
			Out(level, [text](auto& ls) { ls << text; });
		}

		void OutWarning(const char* text) const noexcept
		{
			OutWarning([text](auto& ls) { ls << text; });
		}

		void OutError(const char* text) const noexcept
		{
			OutError([text](auto& ls) { ls << text; });
		}

		void OutFatalError(const char* text) const noexcept
		{
			OutFatalError([text](auto& ls) { ls << text; });
		}
	};

private:
	static Logger* instance;
	MultiPoolAllocator allocator;
	ThreadSafeMultiPoolAllocator inputAlloc;

	/// @brief Identity of the periodic drain task, as issued by the task registry.
	/// @details Not the task itself. The drain task outlives whatever the logger is doing when it is created, and
	///          an ID is what lets the stream notice at run time that the logger has already stopped it rather than
	///          running a subtask for a task nobody owns any more. Null when the logger was started without a task
	///          system, which is a logger that flushes inline.
	TaskID taskID;
	std::atomic<bool> isRunning;
	std::atomic<bool> hasInput;
	std::atomic<bool> needFlush;
	/// @brief True while a thread is inside ProcessBuffer. The swap and text buffers are members, so
	///        exactly one thread may drain at a time - which is what makes an inline drain by a
	///        waiting thread safe rather than a race on member state.
	std::atomic<bool> isDraining;

	TString logPath;
	TLogBuffer inputBuffer;
	TLogBuffer swapBuffer;
	TTextBuffer textBuffer;
	TOutputFuncs flushFuncs;
	TFilters filters;

	std::ofstream outFileStream;
	std::thread::id threadID;

	/// @brief The thread this logger writes on, owned by the logger and by nothing else.
	/// @details Logging has to work before the task system exists, after the task system is gone, and while the
	/// subsystem it
	///          is reporting on is wedged - a logger that depends on the thing it reports cannot report that thing
	///          failing, which is not a preference but the reason this thread exists. Once the task system has built
	///          its streams, the IO stream is handed to this thread, and that stream is driven here through
	///          `TaskStream::Update` rather than owning a thread of its own.
	std::thread driverThread;
	std::atomic<bool> driverRunning{false};
	/// @brief Guards the IO stream pointer, and is held across the pass that uses it.
	/// @details Withdrawal is therefore a lock acquisition: asking for the stream back blocks until whatever the driver
	/// thread
	///          was doing inside the stream has returned, which is the only way the task system can free its streams
	///          without the driver thread reading a freed one on a pass it had already started.
	std::mutex driverLock;
	TaskStream* ioStream = nullptr;

	std::mutex filterLock;
	std::mutex inputLock;

	/// @brief Run one pass of the IO stream on this logger's driver thread, or write the log queue directly when no
	/// stream
	/// @brief has been handed over yet.
	void DriverLoop() noexcept;

public:
	/// @brief Hand the IO stream to this logger's driver thread, or take it back with nullptr.
	/// @details The IO stream is a service stream - the logger is one of its customers, as async file access will be -
	/// and it
	///          is a ride-on-thread stream, so its driver must be named rather than assumed. The task system installs
	///          the stream once its streams exist and withdraws it, waiting for an in-flight pass, before it destroys
	///          them: the driver thread outlives the task system by design, and a pointer left installed would be read
	///          after the streams were freed.
	void SetIODriver(TaskStream* stream) noexcept;

	/// @brief Stop the driver thread, letting it finish what it holds before it exits.
	/// @details Reached only when the process is done logging. The final pass is taken inside the thread itself, so
	/// whatever
	///          was still queued at the moment the stop was requested is written by the thread that owns the file,
	///          rather than abandoned to whoever happened to ask for shutdown.
	void StopDriverThread() noexcept;

	static Logger& Get() noexcept;
	static SimpleLogger Get(StaticString category, ELogLevel level = ELogLevel::Info) noexcept;

public:
	Logger(const Logger&) = delete;
	Logger(Logger&&) = delete;
	Logger& operator=(const Logger&) = delete;
	Logger& operator=(Logger&&) = delete;

public:
	Logger(Engine& engine, const char* path, const char* filename) noexcept;
	~Logger() noexcept;

	[[nodiscard]] static StaticString GetName() noexcept;
	void StartTask(TaskSystem& taskSys);
	void StopTask(TaskSystem& taskSys);

	/// @brief Whether the drain task exists right now - as opposed to whether this object exists.
	/// @details StopTask waits for the drain task to acknowledge the request, so it may only be called
	///          while one is running; called without a task it waits for something that was never
	///          started, and blocks forever. Any caller weighing StopTask against an inline Flush has to
	///          ask this, which is why the answer lives here rather than in a flag elsewhere.
	/// @note Engine::IsLoggerReady is not a substitute: the Logger constructor raises that flag, so it
	///       reports construction and stays true whether or not a task was ever started.
	/// @note Deliberately not the same question AddLog asks before waking the IO stream. That guard asks
	///       whether a stream may be woken and goes false during shutdown, while this task still exists
	///       and still has to be stopped.
	/// @threadsafe May be read from any thread, including a signal handler, because it is a single
	///             acquire load and nothing else.
	[[nodiscard]] bool IsDrainTaskRunning() const noexcept
	{
		return isRunning.load(std::memory_order_acquire);
	}

	void SetFilter(StaticString category, TLogFilter&& filter) noexcept;
	void AddLog(StaticString category, ELogLevel level, const TLogFunction& logFunc) noexcept;

	/// @brief Block until everything already queued has been written out.
	/// @details Drains inline when the caller is the drain thread, and also when no drain task can
	///          run - never started, or already stopped - because then the caller is the only thread
	///          left that could write the queue. Otherwise it waits for the drain task, but only for
	///          a bounded time: a starved or blocked IO thread must not swallow the report that made
	///          the caller flush. On giving up it reports without draining, because draining here while
	///          a drain task is live would race on that task's buffers.
	/// @note Guarantees an empty queue at some instant, not the caller's own prefix: another thread
	///       enqueueing after the queue empties can satisfy the wait first.
	void Flush() noexcept;

#if PROFILE_ENABLED
	void ReportMemoryConfiguration();
#endif // PROFILE_ENABLED

private:
	/// @brief Wait for the drain task to report an empty queue, or drain inline if none can run.
	void WaitForFlush() noexcept;

	/// @brief Write everything currently queued. One thread at a time - see isDraining.
	void ProcessBuffer() noexcept;
	void FlushBuffer(const TTextBuffer& buffer) const noexcept;

	void WriteLog(const TTextBuffer& buffer) noexcept;
	static void PrintStdIO(const TTextBuffer& buffer) noexcept;
};

using TLog = Logger::SimpleLogger;
using LogStream = Logger::TLogStream;

} // namespace hbe

class TaskStream;
