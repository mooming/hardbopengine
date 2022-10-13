// Created by mooming.go@gmail.com, 2022

#pragma once

#include "Log/Logger.h"
#include "Log/LogLevel.h"
#include "Memory/MemoryManager.h"
#include "String/StaticStringTable.h"
#include "System/SystemStatistics.h"
#include "System/TaskSystem.h"
#include <chrono>
#include <fstream>
#include <mutex>


namespace HE
{
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
    std::mutex logLock;
    std::ofstream logFile;

    MemoryManager memoryManager;
    StaticStringTable staticStringTable;
    Logger logger;
    TaskSystem taskSystem;

    SystemStatistics statistics;

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
    StaticString GetName() const;
    inline auto& GetMemoryManager() { return memoryManager; }
    inline auto& GetLogger() { return logger; }
    inline auto& GetTaskSystem() { return taskSystem; }
    inline auto& GetStatistics() const { return statistics; }

    void Log(ELogLevel level, TLogFunc func);
    inline void LogError(TLogFunc func) { Log(ELogLevel::Error, func); }
    void CloseLog();
    void FlushLog();

private:
    void PreUpdate(float deltaTime);
    void Update(float deltaTime);
    void PostUpdate(float deltaTime);
};
} // HE
