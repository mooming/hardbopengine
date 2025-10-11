// Created by mooming.go@gmail.com

#pragma once

#include <fstream>
#include "Core/SystemStatistics.h"
#include "Core/TaskSystem.h"
#include "Log/LogLevel.h"
#include "Log/Logger.h"
#include "Memory/MemoryManager.h"
#include "Resource/ResourceManager.h"

namespace hbe
{

	template<size_t, class>
	class InlineStringBuilder;

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

	public:
		static Engine& Get();

	public:
		Engine(const Engine&) = delete;
		Engine& operator=(const Engine&) = delete;

	public:
		Engine();
		~Engine();

		void Initialize(int argc, const char* argv[]);
		void WaitForEnd();

		// Shut down engine. It'll shut down its task system.
		void ShutDown();

	public:
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
