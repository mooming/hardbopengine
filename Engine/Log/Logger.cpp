// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Logger.h"

#include <algorithm>

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
	void immediateLog(ELogLevel level, StaticString category, const char* logStr)
	{
		AllocatorScope scope(MemoryManager::SystemAllocatorID);

		using namespace std;
		InlineStringBuilder<64> timeStampStr;
		LogUtil::getTimeStampString(timeStampStr);
		auto levelStr = LogUtil::getLogLevelString(level);

		cout << '[' << timeStampStr << "][" << std::this_thread::get_id() << "][" << category << "][" << levelStr
			 << "] " << logStr << endl;
	}
#endif // LOG_FORCE_IMMEDIATE

	void fallbackLog(StaticString category, ELogLevel level, const Logger::TLogFunction& logFunc)
	{
		auto& engine = Engine::get();

		Logger::TLogStream str;
		str << '[' << category << "] ";
		logFunc(str);

		engine.log(level, [&str](auto& ls) { ls << str.c_str(); });
	}

} // anonymous namespace

Logger* Logger::instance = nullptr;

Logger::SimpleLogger::SimpleLogger(StaticString category, ELogLevel level) noexcept : category(category), level(level) {}

void Logger::SimpleLogger::out(const TLogFunction& logFunc) const noexcept
{
	if (unlikely(instance == nullptr))
	{
		fallbackLog(category, level, logFunc);

		return;
	}

	instance->addLog(category, level, logFunc);
}

void Logger::SimpleLogger::out(ELogLevel inLevel, const TLogFunction& logFunc) const noexcept
{
	if (unlikely(instance == nullptr))
	{
		fallbackLog(category, inLevel, logFunc);

		return;
	}

	instance->addLog(category, inLevel, logFunc);
}

Logger& Logger::get() noexcept
{
	fatalAssert(instance != nullptr);

	return *instance;
}

Logger::SimpleLogger Logger::get(StaticString category, ELogLevel level) noexcept
{
	SimpleLogger log(category, level);
	return log;
}

Logger::Logger(Engine& engine, const char* path, const char* filename) noexcept
	: allocator("LoggerMemoryPool"), inputAlloc("LoggerInputPool")
	, task("Logger", nullptr, this)
	, hasInput(false)
	, needFlush(false)
	, logPath(path)
{
	Assert(engine.IsMemoryManagerReady());

	instance = this;
	LogUtil::getStartTime();

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

	auto fileNameSize = StringUtil::strLen(filename, Config::MaxPathLength);
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
	flushFuncs.emplace_back([this](const TTextBuffer& buffer) { writeLog(buffer); });
	flushFuncs.emplace_back([](const TTextBuffer& buffer) { printStdIO(buffer); });

	engine.setLoggerReady();
}

Logger::~Logger() noexcept
{
	instance = nullptr;

	processBuffer();

	outFileStream.flush();
	outFileStream.close();
}

StaticString Logger::getName() noexcept
{
	static auto className = StringUtil::toCompactClassName(__PRETTY_FUNCTION__);
	return className;
}

void Logger::startTask(TaskSystem& taskSys)
{
	addLog(getName(), ELogLevel::Info, [](auto& logStream) { logStream << "Logger started."; });

	auto ioStreamIndex = TaskSystem::getIOTaskStreamIndex();

	auto runnable = [](void* userData,  std::size_t startIndex,  std::size_t endIndex) ->  std::size_t
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

		self->processBuffer();

		return 0;
	};

	isRunning.store(true, std::memory_order_release);

	task.setRunnable(runnable);
	auto rangedTask = task.generateSubTask(0, 1, 0);
	taskSys.enqueue(ioStreamIndex, rangedTask);

	auto& ioTaskStream = taskSys.getIOTaskStream();
	threadID = ioTaskStream.getThreadID();

#if MEMORY_VERIFICATION_ENABLED
	{
		auto& mmgr = MemoryManager::getInstance();
		auto& allocatorProxy = mmgr.getAllocatorProxy(allocator.getID());
		allocatorProxy.threadId = threadID;
	}
#endif // MEMORY_VERIFICATION_ENABLED
}

void Logger::stopTask(TaskSystem& taskSys)
{
	isRunning.store(false, std::memory_order_release);

	if (task.HasDone()) return;

	task.wait();
	threadID = std::thread::id();

#if MEMORY_VERIFICATION_ENABLED
	{
		auto& mmgr = MemoryManager::getInstance();
		auto& allocatorProxy = mmgr.getAllocatorProxy(allocator.getID());
		allocatorProxy.threadId = std::this_thread::get_id();
	}
#endif // MEMORY_VERIFICATION_ENABLED

	addLog(getName(), ELogLevel::Info, [](auto& ls) { ls << "Logger shall be terminated." << hendl; });

	processBuffer();

	outFileStream.flush();
	outFileStream.close();
}

void Logger::addLog(StaticString category, ELogLevel level, const TLogFunction& logFunc) noexcept
{
	Assert(this == instance);

#if !LOG_ENABLED
	return;
#endif // LOG_ENABLED

	AllocatorScope scope(InvalidAllocatorID);

	if (unlikely(logFunc == nullptr))
	{
		addLog(getName(), ELogLevel::Warning, [](auto& ls) { ls << "Null log function!"; });

		return;
	}

	static TAtomicConfigParam<uint8_t> CPLogLevel("Log.Level", "The Default Log Level",
												  static_cast<uint8_t>(ELogLevel::Info));

	if (level < static_cast<ELogLevel>(CPLogLevel.get()))
	{
		return;
	}

	{
		std::lock_guard lock(filterLock);
		auto found = filters.find(category);
		if (found != filters.end())
		{
			auto& filter = found->second;
			if (filter != nullptr && !filter(level)) return;
		}
	}

	auto& engine = Engine::get();
	auto& taskSystem = engine.getTaskSystem();
	auto threadName = TaskSystem::getCurrentStreamName();

	TLogStream ls;
	logFunc(ls);

#if LOG_BREAK_IF_WARNING
	if (unlikely(level >= ELogLevel::Warning))
	{
		immediateLog(level, category, ls.c_str());
		debugBreak();
		return;
	}
#endif // LOG_BREAK_IF_WARNING

#if LOG_BREAK_IF_ERROR
	if (unlikely(level >= ELogLevel::Error))
	{
		immediateLog(level, category, ls.c_str());
		debugBreak();
		return;
	}
#endif // LOG_BREAK_IF_ERROR

#if LOG_FORCE_PRINT_IMMEDIATELY
	immediateLog(level, category, ls.c_str());
	return;
#endif // LOG_FORCE_IMMEDIATE

	if (unlikely(level >= ELogLevel::Error && std::this_thread::get_id() == threadID))
	{
		AllocatorScope memAllocScope(MemoryManager::SystemAllocatorID);

		thread_local TTextBuffer tmpTextBuffer;
		tmpTextBuffer.reserve(1);

		using namespace std;
		InlineStringBuilder<64> timeStampStr;
		LogUtil::getTimeStampString(timeStampStr);

		auto levelStr = LogUtil::getLogLevelString(level);

		InlineStringBuilder<Config::LogOutputBuffer + 128> text;
		text << '[' << timeStampStr.c_str() << "][" << threadName << "][" << category << "][" << levelStr << "] "
			 << ls.c_str();

		tmpTextBuffer.emplace_back(text.c_str());

		flushBuffer(tmpTextBuffer);
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
		flush();
		debugBreak();
		Assert(false);

		return;
	}

	auto& ioStream = taskSystem.getIOTaskStream();
	ioStream.wakeUp();

	if (bufferSize >= Config::LogForceFlushThreshold)
	{
		flush();
	}
}

void Logger::setFilter(StaticString category, TLogFilter&& filter) noexcept
{
	std::lock_guard lock(filterLock);
	filters[category] = std::move(filter);
}

void Logger::flush() noexcept
{
	if (std::this_thread::get_id() == threadID)
	{
		processBuffer();

		return;
	}

	constexpr auto period = std::chrono::milliseconds(10);
	while (needFlush.load(std::memory_order_relaxed))
	{
		std::this_thread::sleep_for(period);
	}
}

#if PROFILE_ENABLED
void Logger::reportMemoryConfiguration() { allocator.reportConfiguration(); }
#endif // PROFILE_ENABLED

void Logger::processBuffer() noexcept
{
	if (!hasInput.load(std::memory_order_acquire)) return;

	AllocatorScope scope(allocator);

	{
		std::lock_guard lockInput(inputLock);
		std::swap(inputBuffer, swapBuffer);
		hasInput.store(false, std::memory_order_release);
	}

	if (swapBuffer.empty()) return;

	bool needIOFlush = false;
	textBuffer.reserve(swapBuffer.size());

	for (auto& log : swapBuffer)
	{
		if (log.level >= ELogLevel::Warning)
		{
			needIOFlush = true;
		}

		InlineStringBuilder<64> timeStampStr;
		LogUtil::getTimeStampString(timeStampStr, log.timeStamp);
		auto levelStr = LogUtil::getLogLevelString(log.level);

		using namespace hbe;
		InlineStringBuilder<Config::LogLineLength * 2> text;

		text << '[' << timeStampStr.c_str() << "][" << log.threadName << "][";
		text << log.category << "][" << levelStr << "] ";
		text << log.getText();

		textBuffer.emplace_back(text.c_str());
	}

	swapBuffer.clear();

	flushBuffer(textBuffer);
	textBuffer.clear();

	if (needIOFlush)
	{
		outFileStream.flush();
	}

	needFlush.store(false, std::memory_order_release);
}

void Logger::flushBuffer(const TTextBuffer& buffer) const noexcept
{
	for (auto& func : flushFuncs)
	{
		Assert(func != nullptr);
		func(buffer);
	}
}

void Logger::writeLog(const TTextBuffer& buffer) noexcept
{
	auto& ofs = outFileStream;

	for (auto& logText : buffer)
	{
		ofs << logText << std::endl;
	}

	ofs.flush();
}

void Logger::printStdIO(const TTextBuffer& buffer) noexcept
{
	auto& engine = Engine::get();

	for (auto& logText : buffer)
	{
		engine.consoleOutLn(logText.c_str());
	}
}

} // namespace hbe
