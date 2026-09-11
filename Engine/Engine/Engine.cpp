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
	void signalHandler(int sigNum)
	{
		using namespace hbe;

		auto& engine = Engine::get();
		engine.getLogger().stopTask(engine.getTaskSystem());
		engine.logError([sigNum](auto& ls) { ls << "ERROR: signal(" << sigNum << ") received. The application shall be terminated."; });
		engine.logError([](auto& ls) {
			auto stackTrace = OS::getBackTrace();
			ls << stackTrace << std::endl;
		});

		engine.logError([](auto& ls) { ls << "Thank you for playing. Have a great day! :)" << std::endl; });
		engine.closeLog();

		exit(128 + sigNum);
	}

} // namespace

namespace hbe
{
	static Engine* engineInstance = nullptr;

	Engine& Engine::get()
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
		std::signal(SIGABRT, signalHandler);
#ifdef SIGBUS
		std::signal(SIGBUS, signalHandler);
#endif // SIGBUS
		std::signal(SIGFPE, signalHandler);
		std::signal(SIGILL, signalHandler);
		std::signal(SIGINT, signalHandler);
		std::signal(SIGSEGV, signalHandler);
		std::signal(SIGTERM, signalHandler);
	}

	Engine::~Engine()
	{
		closeLog();
	}

	void Engine::initialize(int argc, const char* argv[])
	{
		taskSystem.initialize();
		isTaskSystemReady = true;

		logger.startTask(taskSystem);
		isLoggerReady = true;

		auto log = Logger::get(getClassName());

		log.out("Command Line Arguments");

		for (int i = 0; i < argc; ++i)
		{
			log.out([i, argv](auto& ls) { ls << i << " : " << argv[i]; });
		}

		log.out("Engine has been initialized.");

		application = OS::createApplication();
		fatalAssert(application != nullptr);
		application->initialize();

		postInitialize();
	}

	void Engine::run()
	{
		while (taskSystem.GetMainThreadTaskQueue().HasPendingTasks() || taskSystem.IsRunning())
		{
			taskSystem.processMainThreadTasks();
			std::this_thread::yield();
		}

		taskSystem.joinAndClear();
		taskSystem.processMainThreadTasks();

		// It may terminate the application immediately.
		fatalAssert(application != nullptr);
		application.reset();
	}

	void Engine::shutDown()
	{
		// Print final statistics
		{
			auto& configSys = ConfigSystem::get();

#ifdef __DEBUG__
			//        const auto logLevel = static_cast<uint8_t>(ELogLevel::Verbose);
			//        configSys.setByte("Log.Engine", logLevel);
			//        configSys.setByte("Log.Level", logLevel);
#endif // __DEBUG__

			auto& staticStrTable = StaticStringTable::getInstance();
			staticStrTable.printStringTable();
			configSys.printAllParameters();
			statistics.print();
			statistics.printAllocatorProfiles();
		}

		preShutdown();

		auto log = Logger::get(getClassName(), ELogLevel::Info);
		log.out("Shutting down...");

		taskSystem.requestShutDown();
	}

	StaticString Engine::getClassName()
	{
		static StaticString name("Engine");
		return name;
	}

	void Engine::log(ELogLevel level, const TLogFunc& func)
	{
#if ENGINE_LOG_ENABLED
		static TAtomicConfigParam<uint8_t> CPEngineLogLevel("Log.Engine", "The Engine Log Level",
															static_cast<uint8_t>(Config::EngineLogLevel));

		static TAtomicConfigParam<uint8_t> CPEnginePrintLogLevel("Log.Engine.Print",
																 "The Engine Log Level for Standard IO",
																 static_cast<uint8_t>(Config::EngineLogLevelPrint));

		auto levelAsValue = static_cast<uint8_t>(level);
		if (levelAsValue < CPEngineLogLevel.get())
		{
			return;
		}

		using namespace std;

		const auto diff = chrono::steady_clock::now() - statistics.getStartTime();
		auto hours = chrono::duration_cast<chrono::hours>(diff);
		auto minutes = chrono::duration_cast<chrono::minutes>(diff);
		auto seconds = chrono::duration_cast<chrono::seconds>(diff);
		auto milliSeconds = chrono::duration_cast<chrono::milliseconds>(diff);

		auto intHours = hours.count();
		auto intMins = minutes.count() % 60;
		auto intSecs = seconds.count() % 60;
		auto intMSecs = milliSeconds.count() % 1000;

		statistics.incEngineLogCount();

		{
			std::lock_guard lock(logLock);

			if (levelAsValue >= CPEnginePrintLogLevel.get())
			{
				std::stringstream ss;
				ss << '[' << intHours << ':' << intMins << ':' << intSecs << '.' << intMSecs << "] ";

				func(ss);

				consoleOutLn(ss.str().c_str());
				ss.str("");
			}

			Assert(logFile.is_open());

			logFile << '[' << intHours << ':' << intMins << ':' << intSecs << '.' << intMSecs << "] ";

			func(logFile);

			logFile << endl;
		}
#endif // ENGINE_LOG_ENABLED
	}

	void Engine::closeLog()
	{
		if (!logFile.is_open())
		{
			return;
		}

		logFile.flush();
	}

	void Engine::flushLog()
	{
		logger.flush();

		if (!logFile.is_open())
		{
			return;
		}

		logFile.flush();
	}

	void Engine::consoleOutLn(const char* str)
	{
		std::lock_guard lock(consoleOutLock);
		std::cout << str << std::endl;
	}

	void Engine::postInitialize()
	{
		using namespace StringUtil;
		auto log = Logger::get(toCompactMethodName(__PRETTY_FUNCTION__));

		log.out("Engine PostInitialize [Start]");

		memoryManager.postEngineInit();

		log.out("Engine PostInitialize [Done]");
	}

	void Engine::preShutdown()
	{
		using namespace StringUtil;
		auto log = Logger::get(toCompactMethodName(__PRETTY_FUNCTION__));

		log.out("Engine PreShutdown [Start]");

#if PROFILE_ENABLED
		logger.reportMemoryConfiguration();
#endif // PROFILE_ENABLED

		memoryManager.preEngineShutdown();

		log.out("Engine PreShutdown [Done]");
	}

} // namespace hbe
