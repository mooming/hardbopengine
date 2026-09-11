// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <fstream>
#include "Core/SystemStatistics.h"
#include "Core/TaskSystem.h"
#include "Log/LogLevel.h"
#include "Log/Logger.h"
#include "Memory/MemoryManager.h"
#include "OSAL/Application.h"
#include "Resource/ResourceManager.h"

namespace hbe
{

	template<size_t, class>
	class InlineStringBuilder;

	/// @brief Main entry point for the HardBop Engine.
	/// @details Initializes and manages all engine subsystems including memory, logging,
	/// task system, and resources. Acts as the central coordinator for the entire engine.
	class Engine final
	{
	public:
		using TLogFunc = std::function<void(std::ostream& out)>;

	private:
		struct PreEngineInit final
		{
			explicit PreEngineInit(Engine* engine);
		};

		PreEngineInit preEngineInit;

		bool isMemoryManagerReady;
		bool isSystemStatisticsReady;
		bool isLoggerReady;
		bool isTaskSystemReady;
		bool isResourceManagerReady;

		std::mutex logLock;
		std::mutex consoleOutLock;
		std::ofstream logFile;

		MemoryManager memoryManager;
		SystemStatistics statistics;
		Logger logger;
		TaskSystem taskSystem;
		ResourceManager resourceManager;
		std::unique_ptr<OS::Application> application;

	public:
		static Engine& get();

		Engine(const Engine&) = delete;
		Engine& operator=(const Engine&) = delete;

		Engine();
		~Engine();

		void initialize(int argc, const char* argv[]);

		// Should call on main thread.
		void run();

		// Shut down engine. It'll shut down its task system.
		void shutDown();

		static StaticString getClassName();

		auto& getMemoryManager() { return memoryManager; }
		auto& getLogger() { return logger; }
		auto& getTaskSystem() { return taskSystem; }
		auto& getStatistics() { return statistics; }
		auto& getResourceManager() { return resourceManager; }

		void setMemoryManagerReady() { isMemoryManagerReady = true; }
		void setSystemStatisticsReady() { isSystemStatisticsReady = true; }
		void setLoggerReady() { isLoggerReady = true; }
		void setTaskSystemReady() { isTaskSystemReady = true; }
		void setResourceManagerReady() { isResourceManagerReady = true; }

		bool IsMemoryManagerReady() const { return isMemoryManagerReady; }
		bool IsSystemStatisticsReady() const { return isSystemStatisticsReady; }
		bool IsLoggerReady() const { return isLoggerReady; }
		bool IsTaskSystemReady() const { return isTaskSystemReady; }
		bool IsResourceManagerReady() const { return isResourceManagerReady; }

		OS::Application* getApplication() const { return application.get(); }

		void log(ELogLevel level, const TLogFunc& func);
		void logError(const TLogFunc& func) { log(ELogLevel::Error, func); }
		void closeLog();
		void flushLog();

		void consoleOutLn(const char* str);

	private:
		void postInitialize();
		void preShutdown();
	};
} // namespace hbe
