// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Logger.h"

#include <algorithm>
#include <atomic>
#include <cstdio>

#include "../Engine/Engine.h"
#include "Config/BuildConfig.h"
#include "Config/ConfigParam.h"
#include "Config/EngineConfig.h"
#include "Core/Debug.h"
#include "Core/TaskSystem.h"
#include "LogUtil.h"
#include "Memory/AllocatorScope.h"
#include "Memory/InlinePoolAllocator.h"
#include "OSAL/Intrinsic.h"
#include "String/StringUtil.h"

namespace hbe
{
namespace
{
#if LOG_FORCE_PRINT_IMMEDIATELY || LOG_BREAK_IF_WARNING || LOG_BREAK_IF_ERROR
void ImmediateLog(ELogLevel level, StaticString category, const char* logStr)
{
	AllocatorScope scope(MemoryManager::SystemAllocatorID);

	using namespace std;
	InlineStringBuilder<64> timeStampStr;
	LogUtil::GetTimeStampString(timeStampStr);
	auto levelStr = LogUtil::GetLogLevelString(level);

	cout << '[' << timeStampStr << "][" << std::this_thread::get_id() << "][" << category << "][" << levelStr << "] "
		 << logStr << endl;
}
#endif // LOG_FORCE_IMMEDIATE

constexpr int64_t MaxFlushWaitMs = 1000;
constexpr int64_t FlushPollPeriodMs = 10;

void EmergencyLog(StaticString category, ELogLevel level, const Logger::TLogFunction& logFunc)
{
	Logger::TLogStream str;
	str << '[' << category << "][" << LogUtil::GetLogLevelString(level) << "] ";
	logFunc(str);

	auto* out = level >= ELogLevel::Error ? stderr : stdout;
	fputs(str.c_str(), out);
	fputc('\n', out);
	fflush(out);
}

class DrainGuard final
{
public:
	explicit DrainGuard(std::atomic<bool>& inProgress) noexcept
		: guard(inProgress)
		, acquired(!inProgress.exchange(true, std::memory_order_acq_rel))
	{
	}

	~DrainGuard() noexcept
	{
		if (acquired)
		{
			guard.store(false, std::memory_order_release);
		}
	}

	DrainGuard(const DrainGuard&) = delete;
	DrainGuard(DrainGuard&&) = delete;
	DrainGuard& operator=(const DrainGuard&) = delete;
	DrainGuard& operator=(DrainGuard&&) = delete;

	[[nodiscard]] bool IsAcquired() const noexcept
	{
		return acquired;
	}

private:
	std::atomic<bool>& guard;
	bool acquired;
};

} // anonymous namespace

Logger* Logger::instance = nullptr;

Logger::SimpleLogger::SimpleLogger(StaticString category, ELogLevel level) noexcept
	: category(category)
	, level(level)
{
}

void Logger::SimpleLogger::Out(const TLogFunction& logFunc) const noexcept
{
	if (unlikely(instance == nullptr))
	{
		EmergencyLog(category, level, logFunc);

		return;
	}

	instance->AddLog(category, level, logFunc);
}

void Logger::SimpleLogger::Out(ELogLevel inLevel, const TLogFunction& logFunc) const noexcept
{
	if (unlikely(instance == nullptr))
	{
		EmergencyLog(category, inLevel, logFunc);

		return;
	}

	instance->AddLog(category, inLevel, logFunc);
}

Logger& Logger::Get() noexcept
{
	FatalAssert(instance != nullptr);

	return *instance;
}

Logger::SimpleLogger Logger::Get(StaticString category, ELogLevel level) noexcept
{
	SimpleLogger log(category, level);
	return log;
}

Logger::Logger(Engine& engine, const char* path, const char* filename) noexcept
	: allocator("LoggerMemoryPool")
	, inputAlloc("LoggerInputPool")
	, hasInput(false)
	, needFlush(false)
	, isDraining(false)
	, logPath(path)
{
	Assert(engine.IsMemoryManagerReady());

	instance = this;
	LogUtil::GetStartTime();

	AllocatorScope scope(allocator);

	{
		AllocatorScope inputAllocScope(inputAlloc);
		inputBuffer.reserve(16);
		swapBuffer.reserve(16);
	}

	textBuffer.reserve(16);
	filters.reserve(16);

	auto endChar = logPath[logPath.size() - 1];

	{
		auto predicate = [](auto item) { return item == '\\'; };
		std::ranges::replace_if(logPath, predicate, '/');
	}

	auto fileNameSize = StringUtil::StrLen(filename, Config::MaxPathLength);
	if (endChar == '/')
	{
		logPath.reserve(logPath.size() + fileNameSize);
	}
	else
	{
		logPath.reserve(logPath.size() + fileNameSize + 1);
		logPath.push_back('/');
	}

	logPath.append(filename);
	logPath.shrink_to_fit();

	outFileStream.open(logPath.c_str());

	flushFuncs.reserve(2);
	flushFuncs.emplace_back([this](const TTextBuffer& buffer) { WriteLog(buffer); });
	flushFuncs.emplace_back([](const TTextBuffer& buffer) { PrintStdIO(buffer); });

	// The driver thread starts with the logger itself, not with the first subsystem that wants to write a line.
	driverRunning.store(true, std::memory_order_release);
	driverThread = std::thread([this] { DriverLoop(); });
	threadID = driverThread.get_id();

	engine.SetLoggerReady();
}

void Logger::StopDriverThread() noexcept
{
	driverRunning.store(false, std::memory_order_release);

	if (driverThread.joinable() && driverThread.get_id() != std::this_thread::get_id())
	{
		driverThread.join();
	}
}

void Logger::DriverLoop() noexcept
{
	threadID = std::this_thread::get_id();
	TaskSystem::SetThreadName("LogDriver");

	// One line from this thread, so that the name it was given can be seen in a log rather than assumed from the call that
	// set it: every log line is attributed to the thread that wrote it, so a thread which never logs is a thread whose name
	// is never observable.
	Logger::Get(GetName()).Out("Log driver thread is running.");

	while (driverRunning.load(std::memory_order_acquire))
	{
		// With a stream installed, the IO stream's own queues are the work: whatever the logger posted there runs,
		// along with any other customer's IO request. Without one - before the task system exists and after it is gone
		// - the log queue is written directly, which is the same work with one fewer hop.
		TaskStream* stream = nullptr;

		{
			std::lock_guard lock(driverLock);
			stream = ioStream;

			if (stream != nullptr)
			{
				stream->Update();
			}
		}

		// Waiting on the stream, rather than sleeping and trying again, is what makes the thread cheap when nothing is happening
		// and immediate when something is: a log line already wakes this stream, so the same wake-up serves both customers of
		// one thread. The timeout is a safety net, not the mechanism - the wake-up notification is issued without the queue lock
		// held, so a lost one costs at most this interval rather than a stall that never ends.
		if (stream != nullptr)
		{
			stream->WaitForWork(std::chrono::milliseconds(20));
		}

		if (stream == nullptr)
		{
			ProcessBuffer();

			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

	std::lock_guard lock(driverLock);

	if (ioStream != nullptr)
	{
		while (ioStream->Update())
		{
		}
	}

	ProcessBuffer();
}

void Logger::SetIODriver(TaskStream* stream) noexcept
{
	// Passing the lock is what makes withdrawal mean something: the new owner cannot install a different stream, and
	// nullptr cannot land, while a pass is in flight.
	std::lock_guard lock(driverLock);
	ioStream = stream;

	// Withdrawing the stream also withdraws the drain task that lived on it: nothing can run it any more, so leaving the flag
	// set would have every later flush wait 1000ms on an executor that no longer exists, then report the loss and assert. The
	// driver loop writes the queue directly from here on, which is the same work with one fewer hop.
	if (stream == nullptr)
	{
		isRunning.store(false, std::memory_order_release);
	}
}

Logger::~Logger() noexcept
{
	StopDriverThread();
	instance = nullptr;

	ProcessBuffer();

	outFileStream.flush();
	outFileStream.close();
}

StaticString Logger::GetName() noexcept
{
	static auto className = StringUtil::ToCompactClassName(__PRETTY_FUNCTION__);
	return className;
}

void Logger::StartTask(TaskSystem& taskSys)
{
	AddLog(GetName(), ELogLevel::Info, [](auto& logStream) { logStream << "Logger started."; });

	auto ioStreamIndex = TaskSystem::GetIOTaskStreamIndex();

	auto runnable = [](void* userData, std::size_t startIndex, std::size_t endIndex) -> std::size_t
	{
		auto self = static_cast<Logger*>(userData);
		if (self == nullptr)
		{
			Assert(false, "Invalid userdata %p", userData);
			return 1;
		}

		const bool isRunning = self->isRunning.load(std::memory_order_relaxed);
		if (!isRunning)
		{
			return 1;
		}

		self->ProcessBuffer();

		return 0;
	};

	isRunning.store(true, std::memory_order_release);

	taskID = taskSys.CreateTask("Logger", runnable, this);

	auto* drainTask = taskSys.FindTask(taskID);
	if (drainTask == nullptr)
	{
		AddLog(GetName(), ELogLevel::Error, [](auto& logStream)
		{
			logStream << "The task registry could not track the logger's drain task, so log lines are flushed"
					  << " inline by whoever produces them.";
		});
		return;
	}

	taskSys.EnqueueTask(ioStreamIndex, *drainTask);

	auto& ioTaskStream = taskSys.GetIOTaskStream();
	threadID = ioTaskStream.GetThreadID();

#if MEMORY_VERIFICATION_ENABLED
	{
		auto& mmgr = MemoryManager::GetInstance();
		auto& allocatorProxy = mmgr.GetAllocatorProxy(allocator.GetID());
		allocatorProxy.threadId = threadID;
	}
#endif // MEMORY_VERIFICATION_ENABLED
}

void Logger::StopTask(TaskSystem& taskSys)
{
	isRunning.store(false, std::memory_order_release);

	// The drain work is one item in the IO stream's queue, so an empty queue is the only observable that says the promise has
	// been kept. Waiting on the task instead spun on a counter that cannot distinguish finished from never-dispatched, and spun
	// forever when no executor was left to run it - a hang dressed as a wait. On give-up the task is deliberately not released:
	// freeing a task another thread may still run is a use-after-free, not a shutdown.
	auto& drainStream = taskSys.GetIOTaskStream();
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(1000);

	while (drainStream.CountPendingItems() > 0 && std::chrono::steady_clock::now() < deadline)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	if (drainStream.CountPendingItems() > 0)
	{
		AddLog(GetName(), ELogLevel::Error, [](auto& logStream)
		{
			logStream << "Logger gave up waiting for its drain task and left it alive: the IO stream still held work after "
					"1000ms. Releasing it here would free a task another thread might still run.";
		});

		return;
	}

	taskSys.ReleaseTask(taskID);
	threadID = std::thread::id();

#if MEMORY_VERIFICATION_ENABLED
	{
		auto& mmgr = MemoryManager::GetInstance();
		auto& allocatorProxy = mmgr.GetAllocatorProxy(allocator.GetID());
		allocatorProxy.threadId = std::this_thread::get_id();
	}
#endif // MEMORY_VERIFICATION_ENABLED

	AddLog(GetName(), ELogLevel::Info, [](auto& ls) { ls << "Logger shall be terminated." << hendl; });

	ProcessBuffer();

	outFileStream.flush();
	outFileStream.close();
}

void Logger::AddLog(StaticString category, ELogLevel level, const TLogFunction& logFunc) noexcept
{
	Assert(this == instance);

#if !LOG_ENABLED
	return;
#endif // LOG_ENABLED

	AllocatorScope scope(InvalidAllocatorID);

	if (unlikely(logFunc == nullptr))
	{
		AddLog(GetName(), ELogLevel::Warning, [](auto& ls) { ls << "Null log function!"; });

		return;
	}

	static TAtomicConfigParam<uint8_t> CPLogLevel("Log.Level", "The Default Log Level",
												  static_cast<uint8_t>(ELogLevel::Info));

	if (level < static_cast<ELogLevel>(CPLogLevel.Get()))
	{
		return;
	}

	{
		std::lock_guard lock(filterLock);
		auto found = filters.find(category);
		if (found != filters.end())
		{
			auto& filter = found->second;
			if (filter != nullptr && !filter(level))
				return;
		}
	}

	auto& engine = Engine::Get();
	auto& taskSystem = engine.GetTaskSystem();
	auto threadName = TaskSystem::GetCurrentStreamName();

	TLogStream ls;
	logFunc(ls);

#if LOG_BREAK_IF_WARNING
	if (unlikely(level >= ELogLevel::Warning))
	{
		ImmediateLog(level, category, ls.c_str());
		debugBreak();
		return;
	}
#endif // LOG_BREAK_IF_WARNING

#if LOG_BREAK_IF_ERROR
	if (unlikely(level >= ELogLevel::Error))
	{
		ImmediateLog(level, category, ls.c_str());
		debugBreak();
		return;
	}
#endif // LOG_BREAK_IF_ERROR

#if LOG_FORCE_PRINT_IMMEDIATELY
	ImmediateLog(level, category, ls.c_str());
	return;
#endif // LOG_FORCE_IMMEDIATE

	if (unlikely(level >= ELogLevel::Error && std::this_thread::get_id() == threadID))
	{
		AllocatorScope memAllocScope(MemoryManager::SystemAllocatorID);

		thread_local TTextBuffer tmpTextBuffer;
		tmpTextBuffer.reserve(1);

		using namespace std;
		InlineStringBuilder<64> timeStampStr;
		LogUtil::GetTimeStampString(timeStampStr);

		auto levelStr = LogUtil::GetLogLevelString(level);

		InlineStringBuilder<Config::LogOutputBuffer + 128> text;
		text << '[' << timeStampStr.c_str() << "][" << threadName << "][" << category << "][" << levelStr << "] "
			 << ls.c_str();

		tmpTextBuffer.emplace_back(text.c_str());

		FlushBuffer(tmpTextBuffer);
		tmpTextBuffer.clear();

		if (unlikely(level >= ELogLevel::FatalError))
		{
			debugBreak();
		}

		return;
	}

	size_t bufferSize = 0;

	{
		std::lock_guard lock(inputLock);
		AllocatorScope inputAllocScope(inputAlloc);
		inputBuffer.emplace_back(level, threadName, category, ls.c_str(), ls.Size());

		bufferSize = inputBuffer.size();
		hasInput.store(true, std::memory_order_release);
		needFlush.store(true, std::memory_order_release);
	}

	if (unlikely(level >= ELogLevel::FatalError))
	{
		Flush();
		debugBreak();
		Assert(false);

		return;
	}

	if (taskSystem.IsRunning())
	{
		taskSystem.GetIOTaskStream().WakeUp();
	}

	if (bufferSize >= Config::LogForceFlushThreshold)
	{
		Flush();
	}
}

void Logger::SetFilter(StaticString category, TLogFilter&& filter) noexcept
{
	std::lock_guard lock(filterLock);
	filters[category] = std::move(filter);
}

void Logger::Flush() noexcept
{
	if (std::this_thread::get_id() == threadID)
	{
		ProcessBuffer();

		return;
	}

	WaitForFlush();
}

void Logger::WaitForFlush() noexcept
{
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(MaxFlushWaitMs);

	while (needFlush.load(std::memory_order_relaxed))
	{
		const bool hasDrainTask = isRunning.load(std::memory_order_acquire);

		if (!hasDrainTask)
		{
			ProcessBuffer();
		}

		if (std::chrono::steady_clock::now() >= deadline)
		{
			if (hasDrainTask)
			{
				EmergencyLog(GetName(), ELogLevel::Error, [](auto& ls)
				{
					ls << "Flush gave up after " << MaxFlushWaitMs
					   << "ms: the drain task is alive but has not written the queue. The entries stay "
					   << "buffered, and this report was written straight to standard error.";
				});
			}
			else
			{
				EmergencyLog(GetName(), ELogLevel::Error, [](auto& ls)
				{
					ls << "Flush gave up after " << MaxFlushWaitMs
					   << "ms with no drain task to write the queue. The entries stay buffered, and this "
					   << "report was written straight to standard error.";
				});
			}

			return;
		}

		if (needFlush.load(std::memory_order_relaxed))
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(FlushPollPeriodMs));
		}
	}
}

#if PROFILE_ENABLED
void Logger::ReportMemoryConfiguration()
{
	allocator.ReportConfiguration();
}
#endif // PROFILE_ENABLED

void Logger::ProcessBuffer() noexcept
{
	const DrainGuard drain(isDraining);
	if (!drain.IsAcquired())
		return;

	if (!hasInput.load(std::memory_order_acquire))
	{
		needFlush.store(false, std::memory_order_release);

		return;
	}

	AllocatorScope scope(allocator);

	{
		std::lock_guard lockInput(inputLock);
		std::swap(inputBuffer, swapBuffer);
		hasInput.store(false, std::memory_order_release);
	}

	if (swapBuffer.empty())
	{
		needFlush.store(false, std::memory_order_release);

		return;
	}

	bool needIOFlush = false;
	textBuffer.reserve(swapBuffer.size());

	for (auto& log : swapBuffer)
	{
		if (log.level >= ELogLevel::Warning)
		{
			needIOFlush = true;
		}

		InlineStringBuilder<64> timeStampStr;
		LogUtil::GetTimeStampString(timeStampStr, log.timeStamp);
		auto levelStr = LogUtil::GetLogLevelString(log.level);

		using namespace hbe;
		InlineStringBuilder<Config::LogLineLength * 2> text;

		text << '[' << timeStampStr.c_str() << "][" << log.threadName << "][";
		text << log.category << "][" << levelStr << "] ";
		text << log.GetText();

		textBuffer.emplace_back(text.c_str());
	}

	swapBuffer.clear();

	FlushBuffer(textBuffer);
	textBuffer.clear();

	if (needIOFlush)
	{
		outFileStream.flush();
	}

	needFlush.store(false, std::memory_order_release);
}

void Logger::FlushBuffer(const TTextBuffer& buffer) const noexcept
{
	for (auto& func : flushFuncs)
	{
		Assert(func != nullptr);
		func(buffer);
	}
}

void Logger::WriteLog(const TTextBuffer& buffer) noexcept
{
	auto& ofs = outFileStream;

	for (auto& logText : buffer)
	{
		ofs << logText << std::endl;
	}

	ofs.flush();
}

void Logger::PrintStdIO(const TTextBuffer& buffer) noexcept
{
	auto& engine = Engine::Get();

	for (auto& logText : buffer)
	{
		engine.ConsoleOutLn(logText.c_str());
	}
}

} // namespace hbe
