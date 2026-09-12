// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Engine.h"

#include <csignal>
#include <iostream>
#include <thread>
#include "Config/ConfigParam.h"
#include "Config/ConfigSystem.h"
#include "Core/Debug.h"
#include "Core/ScopedLock.h"
#include "String/StaticStringTable.h"
#include "OSAL/OSDebug.h"

namespace
{
	void SignalHandler(int sigNum)
	{
		using namespace hbe;

		auto& engine = Engine::Get();
		engine.GetLogger().StopTask(engine.GetTaskSystem());
		engine.LogError([sigNum](auto& ls) { ls << "ERROR: signal(" << sigNum << ") received. The application shall be terminated."; });
		engine.LogError([](auto& ls) {
			auto stackTrace = OS::GetBackTrace();
			ls << stackTrace << std::endl;
		});

		engine.LogError([](auto& ls) { ls << "Thank you for playing. Have a great day! :)" << std::endl; });
		engine.CloseLog();

		exit(128 + sigNum);
	}

} // namespace

namespace hbe
{
	static Engine* engineInstance = nullptr;

	Engine& Engine::Get()
	{
		Assert(engineInstance != nullptr);
		return *engineInstance;
	}

	Engine::PreEngineInit::PreEngineInit(Engine* engine) { engineInstance = engine; }

	Engine::Engine()
		: preEngineInit(this)
		, isMemoryManagerReady(false)
		, isSystemStatisticsReady(false)
		, isLoggerReady(false)
		, isTaskSystemReady(false)
		, isResourceManagerReady(false)
		, logFile("helowlevel.log")
		, memoryManager(*this)
		, statistics(*this)
		, logger(*this, "./", "hardbop.log")
	{
		std::signal(SIGABRT, SignalHandler);
#ifdef SIGBUS
		std::signal(SIGBUS, SignalHandler);
#endif // SIGBUS
		std::signal(SIGFPE, SignalHandler);
		std::signal(SIGILL, SignalHandler);
		std::signal(SIGINT, SignalHandler);
		std::signal(SIGSEGV, SignalHandler);
		std::signal(SIGTERM, SignalHandler);
	}

	Engine::~Engine()
	{
		CloseLog();
	}

	void Engine::Initialize(int argc, const char* argv[])
	{
		taskSystem.Initialize();
		isTaskSystemReady = true;

		logger.StartTask(taskSystem);
		isLoggerReady = true;

		auto log = Logger::Get(GetClassName());

		log.Out("Command Line Arguments");

		for (int i = 0; i < argc; ++i)
		{
			log.Out([i, argv](auto& ls) { ls << i << " : " << argv[i]; });
		}

		log.Out("Engine has been initialized.");

		application = OS::CreateApplication();
		FatalAssert(application != nullptr);
		application->Initialize();

		PostInitialize();
	}

	void Engine::Run()
	{
		while (taskSystem.GetMainThreadTaskQueue().HasPendingTasks() || taskSystem.IsRunning())
		{
			taskSystem.ProcessMainThreadTasks();
			std::this_thread::yield();
		}

		taskSystem.JoinAndClear();
		taskSystem.ProcessMainThreadTasks();

		// It may terminate the application immediately.
		FatalAssert(application != nullptr);
		application.reset();
	}

	void Engine::ShutDown()
	{
		// Print final statistics
		{
			auto& configSys = ConfigSystem::Get();

#ifdef __DEBUG__
			//        const auto logLevel = static_cast<uint8_t>(ELogLevel::Verbose);
			//        configSys.SetByte("Log.Engine", logLevel);
			//        configSys.SetByte("Log.Level", logLevel);
#endif // __DEBUG__

			auto& staticStrTable = StaticStringTable::GetInstance();
			staticStrTable.PrintStringTable();
			configSys.PrintAllParameters();
			statistics.Print();
			statistics.PrintAllocatorProfiles();
		}

		PreShutdown();

		auto log = Logger::Get(GetClassName(), ELogLevel::Info);
		log.Out("Shutting down...");

		taskSystem.RequestShutDown();
	}

	StaticString Engine::GetClassName()
	{
		static StaticString name("Engine");
		return name;
	}

	void Engine::Log(ELogLevel level, const TLogFunc& func)
	{
#if ENGINE_LOG_ENABLED
		static TAtomicConfigParam<uint8_t> CPEngineLogLevel("Log.Engine", "The Engine Log Level",
															static_cast<uint8_t>(Config::EngineLogLevel));

		static TAtomicConfigParam<uint8_t> CPEnginePrintLogLevel("Log.Engine.Print",
																 "The Engine Log Level for Standard IO",
																 static_cast<uint8_t>(Config::EngineLogLevelPrint));

		auto levelAsValue = static_cast<uint8_t>(level);
		if (levelAsValue < CPEngineLogLevel.Get())
		{
			return;
		}

		using namespace std;

		const auto diff = chrono::steady_clock::now() - statistics.GetStartTime();
		auto hours = chrono::duration_cast<chrono::hours>(diff);
		auto minutes = chrono::duration_cast<chrono::minutes>(diff);
		auto seconds = chrono::duration_cast<chrono::seconds>(diff);
		auto milliSeconds = chrono::duration_cast<chrono::milliseconds>(diff);

		auto intHours = hours.count();
		auto intMins = minutes.count() % 60;
		auto intSecs = seconds.count() % 60;
		auto intMSecs = milliSeconds.count() % 1000;

		statistics.IncEngineLogCount();

		{
			std::lock_guard lock(logLock);

			if (levelAsValue >= CPEnginePrintLogLevel.Get())
			{
				std::stringstream ss;
				ss << '[' << intHours << ':' << intMins << ':' << intSecs << '.' << intMSecs << "] ";

				func(ss);

				ConsoleOutLn(ss.str().c_str());
				ss.str("");
			}

			Assert(logFile.is_open());

			logFile << '[' << intHours << ':' << intMins << ':' << intSecs << '.' << intMSecs << "] ";

			func(logFile);

			logFile << endl;
		}
#endif // ENGINE_LOG_ENABLED
	}

	void Engine::CloseLog()
	{
		if (!logFile.is_open())
		{
			return;
		}

		logFile.flush();
	}

	void Engine::FlushLog()
	{
		logger.Flush();

		if (!logFile.is_open())
		{
			return;
		}

		logFile.flush();
	}

	void Engine::ConsoleOutLn(const char* str)
	{
		std::lock_guard lock(consoleOutLock);
		std::cout << str << std::endl;
	}

	void Engine::PostInitialize()
	{
		using namespace StringUtil;
		auto log = Logger::Get(ToCompactMethodName(__PRETTY_FUNCTION__));

		log.Out("Engine PostInitialize [Start]");

		memoryManager.PostEngineInit();

		log.Out("Engine PostInitialize [Done]");
	}

	void Engine::PreShutdown()
	{
		using namespace StringUtil;
		auto log = Logger::Get(ToCompactMethodName(__PRETTY_FUNCTION__));

		log.Out("Engine PreShutdown [Start]");

#if PROFILE_ENABLED
		logger.ReportMemoryConfiguration();
#endif // PROFILE_ENABLED

		memoryManager.PreEngineShutdown();

		log.Out("Engine PreShutdown [Done]");
	}

} // namespace hbe
