// Created by mooming.go@gmail.com, 2022

#pragma once

#include "Log/Logger.h"
#include "Log/LogLevel.h"
#include "Memory/MemoryManager.h"
#include "String/StaticStringTable.h"
#include <chrono>
#include <fstream>
#include <mutex>


namespace HE
{
    class Engine final
    {
    public:
        using TLogFunc = std::function<void(std::ostream& out)>;
        using TTimePoint = std::chrono::time_point<std::chrono::steady_clock>;
        
    private:
        struct PreEngineInit final
        {
            PreEngineInit(Engine* engine);
        };
        
        PreEngineInit preEngineInit;
        std::mutex logLock;
        std::fstream logFile;
        TTimePoint startTime;
        
        MemoryManager memoryManager;
        StaticStringTable staticStringTable;
        Logger logger;
        
    public:
        static Engine& Get();
        
    public:
        Engine();
        ~Engine();

        void Initialize(int argc, const char* argv[]);
        void Run();
        
        StaticString GetName() const;
        inline auto& GetMemoryManager() { return memoryManager; }
        inline auto& GetLogger() { return logger; }
        
        void Log(ELogLevel level, TLogFunc func);
        inline void LogError(TLogFunc func) { Log(ELogLevel::Error, func); }
        
        void CloseLog();
    };
}
