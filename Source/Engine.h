// Created by mooming.go@gmail.com, 2022

#pragma once

#include "Config/BuildConfig.h"
#include "Config/EngineSettings.h"
#include "Log/Logger.h"
#include "Log/LogLevel.h"
#include "Memory/MemoryManager.h"
#include "Resource/ResourceManager.h"
#include "System/SystemStatistics.h"
#include "System/TaskSystem.h"
#include <chrono>
#include <fstream>
#include <mutex>


namespace HE
{

template <size_t, class>
class InlineStringBuilder;

class Engine final
{
public:
    using TLogFunc = std::function<void(std::ostream& out)>;
    
private:
    struct PreEngineInit final
    {
        PreEngineInit(Engine* engine);
    };

    PreEngineInit preEngineInit;

    bool isRunning;
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
    Engine& operator= (const Engine&) = delete;

public:
    Engine();
    ~Engine();

    void Initialize(int argc, const char* argv[]);
    void Run();
    void Stop();

public:
    StaticString GetClassName() const;

    inline auto& GetMemoryManager() { return memoryManager; }
    inline auto& GetLogger() { return logger; }
    inline auto& GetTaskSystem() { return taskSystem; }
    inline auto& GetStatistics() { return statistics; }
    inline auto& GetResourceManager() { return resourceManager; }

    inline void SetMemoryManagerReady() { isMemoryManagerReady = true; }
    inline void SetSystemStatisticsReady() { isSystemStatisticsReady = true; }
    inline void SetLoggerReady() { isLoggerReady = true; }
    inline void SetTaskSystemReady() { isTaskSystemReady = true; }
    inline void SetResourceManagerReady() { isResourceManagerReady = true; }

    inline bool IsMemoryManagerReady() const { return isMemoryManagerReady; }
    inline bool IsSystemStatisticsReady() const { return isSystemStatisticsReady; }
    inline bool IsLoggerReady() const { return isLoggerReady; }
    inline bool IsTaskSystemReady() const { return isTaskSystemReady; }
    inline bool IsResourceManagerReady() const { return isResourceManagerReady; }

    void Log(ELogLevel level, TLogFunc func);
    inline void LogError(TLogFunc func) { Log(ELogLevel::Error, func); }
    void CloseLog();
    void FlushLog();

    void ConsoleOutLn(const char* str);

private:
    void PostInitialize();

    void PreUpdate(float deltaTime);
    void Update(float deltaTime);
    void PostUpdate(float deltaTime);

    void PreShutdown();
};
} // HE
