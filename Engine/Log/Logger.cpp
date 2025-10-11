// Created by mooming.go@gmail.com

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
		void ImmediateLog(ELogLevel level, StaticString category, const char* logStr)
		{
			AllocatorScope scope(MemoryManager::SystemAllocatorID);

			using namespace std;
			InlineStringBuilder<64> timeStampStr;
			LogUtil::GetTimeStampString(timeStampStr);
			auto levelStr = LogUtil::GetLogLevelString(level);

			cout << '[' << timeStampStr << "][" << std::this_thread::get_id() << "][" << category << "][" << levelStr
				 << "] " << logStr << endl;
		}
#endif // LOG_FORCE_IMMEDIATE

		void FallbackLog(StaticString category, ELogLevel level, const Logger::TLogFunction& logFunc)
		{
			auto& engine = Engine::Get();

			Logger::TLogStream str;
			str << '[' << category << "] ";
			logFunc(str);

			engine.Log(level, [&str](auto& ls) { ls << str.c_str(); });
		}

	} // anonymous namespace

	Logger* Logger::instance = nullptr;

	Logger::SimpleLogger::SimpleLogger(StaticString category, ELogLevel level) : category(category), level(level) {}

	void Logger::SimpleLogger::Out(const TLogFunction& logFunc) const
	{
		if (unlikely(instance == nullptr))
		{
			FallbackLog(category, level, logFunc);
			return;
		}

		instance->AddLog(category, level, logFunc);
	}

	void Logger::SimpleLogger::Out(ELogLevel inLevel, const TLogFunction& logFunc) const
	{
		if (unlikely(instance == nullptr))
		{
			FallbackLog(category, inLevel, logFunc);
			return;
		}

		instance->AddLog(category, inLevel, logFunc);
	}

	Logger& Logger::Get()
	{
		FatalAssert(instance != nullptr);

		return *instance;
	}

	Logger::SimpleLogger Logger::Get(StaticString category, ELogLevel level)
	{
		SimpleLogger log(category, level);
		return log;
	}

	Logger::Logger(Engine& engine, const char* path, const char* filename)
		: allocator("LoggerMemoryPool"), inputAlloc("LoggerInputPool")
		, task("Logger", nullptr, this)
		, hasInput(false)
		, needFlush(false)
	{
		Assert(engine.IsMemoryManagerReady());

		instance = this;
		LogUtil::GetStartTime();

		AllocatorScope scope(allocator);

		logPath = path;

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

		engine.SetLoggerReady();
	}

	Logger::~Logger()
	{
		instance = nullptr;

		ProcessBuffer();

		outFileStream.flush();
		outFileStream.close();
	}

	StaticString Logger::GetName()
	{
		static auto className = StringUtil::ToCompactClassName(__PRETTY_FUNCTION__);
		return className;
	}

	void Logger::StartTask(TaskSystem& taskSys)
	{
		AddLog(GetName(), ELogLevel::Info, [](auto& logStream) { logStream << "Logger started."; });

		auto ioStreamIndex = TaskSystem::GetIOTaskStreamIndex();

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

			self->ProcessBuffer();

			return 0;
		};

		isRunning.store(true, std::memory_order_release);

		task.SetRunnable(runnable);
		auto rangedTask = task.GenerateSubTask(0, 1, 0);
		taskSys.Enqueue(ioStreamIndex, rangedTask);

		auto& ioTaskStream = taskSys.GetIOTaskStream();
		threadID = ioTaskStream.GetThreadID();

#if __MEMORY_VERIFICATION__
		{
			auto& mmgr = MemoryManager::GetInstance();
			auto& allocatorProxy = mmgr.GetAllocatorProxy(allocator.GetID());
			allocatorProxy.threadId = threadID;
		}
#endif // __MEMORY_VERIFICATION__
	}

	void Logger::StopTask(TaskSystem& taskSys)
	{
		isRunning.store(false, std::memory_order_release);

		if (task.HasDone())
		{
			return;
		}

		task.Wait();
		threadID = std::thread::id();

#if __MEMORY_VERIFICATION__
		{
			auto& mmgr = MemoryManager::GetInstance();
			auto& allocatorProxy = mmgr.GetAllocatorProxy(allocator.GetID());
			allocatorProxy.threadId = std::this_thread::get_id();
		}
#endif // __MEMORY_VERIFICATION__

		AddLog(GetName(), ELogLevel::Info, [](auto& ls) { ls << "Logger shall be terminated." << hendl; });

		ProcessBuffer();

		outFileStream.flush();
		outFileStream.close();
	}

	void Logger::AddLog(StaticString category, ELogLevel level, const TLogFunction& logFunc)
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
				{
					return;
				}
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

			static TTextBuffer tmpTextBuffer;
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

		auto& ioStream = taskSystem.GetIOTaskStream();
		ioStream.WakeUp();

		if (bufferSize >= Config::LogForceFlushThreshold)
		{
			Flush();
		}
	}

	void Logger::SetFilter(StaticString category, TLogFilter&& filter)
	{
		std::lock_guard lock(filterLock);
		filters[category] = std::move(filter);
	}

	void Logger::Flush()
	{
		if (std::this_thread::get_id() == threadID)
		{
			ProcessBuffer();
			return;
		}

		constexpr auto period = std::chrono::milliseconds(10);
		while (needFlush.load(std::memory_order_relaxed))
		{
			std::this_thread::sleep_for(period);
		}
	}

#if PROFILE_ENABLED
	void Logger::ReportMemoryConfiguration() { allocator.ReportConfiguration(); }
#endif // PROFILE_ENABLED

	void Logger::ProcessBuffer()
	{
		if (!hasInput.load(std::memory_order_acquire))
		{
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
			return;
		}

		bool bNeedIOFlush = false;
		textBuffer.reserve(swapBuffer.size());

		for (auto& log : swapBuffer)
		{
			if (log.level >= ELogLevel::Warning)
			{
				bNeedIOFlush = true;
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

		if (bNeedIOFlush)
		{
			outFileStream.flush();
		}

		needFlush.store(false, std::memory_order_release);
	}

	void Logger::FlushBuffer(const TTextBuffer& buffer) const
	{
		for (auto& func : flushFuncs)
		{
			Assert(func != nullptr);
			func(buffer);
		}
	}

	void Logger::WriteLog(const TTextBuffer& buffer)
	{
		auto& ofs = outFileStream;

		for (auto& logText : buffer)
		{
			ofs << logText << std::endl;
		}

		ofs.flush();
	}

	void Logger::PrintStdIO(const TTextBuffer& buffer)
	{
		auto& engine = Engine::Get();

		for (auto& logText : buffer)
		{
			engine.ConsoleOutLn(logText.c_str());
		}
	}

} // namespace hbe
