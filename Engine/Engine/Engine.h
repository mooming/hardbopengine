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
		std::unique_ptr<OS::IApplication> application;

	public:
		static Engine& Get();

		Engine(const Engine&) = delete;
		Engine& operator=(const Engine&) = delete;

		Engine();
		~Engine();

		void Initialize(int argc, const char* argv[]);

		// Should call on main thread.
		void Run();

		// Shut down engine. It'll shut down its task system.
		void ShutDown();

		static StaticString GetClassName();

		auto& GetMemoryManager() { return memoryManager; }
		auto& GetLogger() { return logger; }
		auto& GetTaskSystem() { return taskSystem; }
		auto& GetStatistics() { return statistics; }
		auto& GetResourceManager() { return resourceManager; }

		void SetMemoryManagerReady() { isMemoryManagerReady = true; }
		void SetSystemStatisticsReady() { isSystemStatisticsReady = true; }
		void SetLoggerReady() { isLoggerReady = true; }
		void SetTaskSystemReady() { isTaskSystemReady = true; }
		void SetResourceManagerReady() { isResourceManagerReady = true; }

		bool IsMemoryManagerReady() const { return isMemoryManagerReady; }
		bool IsSystemStatisticsReady() const { return isSystemStatisticsReady; }
		bool IsLoggerReady() const { return isLoggerReady; }
		bool IsTaskSystemReady() const { return isTaskSystemReady; }
		bool IsResourceManagerReady() const { return isResourceManagerReady; }

		OS::IApplication* GetApplication() const { return application.get(); }

		void Log(ELogLevel level, const TLogFunc& func);
		void LogError(const TLogFunc& func) { Log(ELogLevel::Error, func); }
		void CloseLog();
		void FlushLog();

		void ConsoleOutLn(const char* str);

	private:
		void PostInitialize();
		void PreShutdown();
	};
} // namespace hbe
