// Created by mooming.go@gmail.com 2022

#pragma once

#include "LogLevel.h"
#include "LogLine.h"
#include "HSTL/HString.h"
#include "HSTL/HVector.h"
#include "HSTL/HUnorderedMap.h"
#include "Memory/MultiPoolAllocator.h"
#include "String/InlineStringBuilder.h"
#include "String/StaticString.h"
#include "System/TaskHandle.h"
#include <atomic>
#include <chrono>
#include <fstream>
#include <functional>
#include <mutex>
#include <thread>


namespace HE
{

class Engine;
class TaskSystem;

class Logger final
{
public:
    using TString = HSTL::HString;
    using TLogBuffer = HSTL::HVector<LogLine>;
    using TTextBuffer = HSTL::HVector<TString>;
    using TTimePoint = std::chrono::time_point<std::chrono::steady_clock>;
    using TLogStream = InlineStringBuilder<Config::LogOutputBuffer>;
    using TLogFunction = std::function<void(TLogStream&)>;
    using TOutputFunc = std::function<void(const TTextBuffer&)>;
    using TOutputFuncs = HSTL::HVector<TOutputFunc>;
    using TLogFilter = std::function<bool(ELogLevel)>;
    using TFilters = HSTL::HUnorderedMap<StaticString, TLogFilter>;

public:
    struct SimpleLogger final
    {
        const StaticString category;
        const ELogLevel level;

        SimpleLogger(StaticString category, ELogLevel level = ELogLevel::Info);
        void Out(const TLogFunction& logFunc) const ;
        void Out(ELogLevel level, const TLogFunction& logFunc) const;
        
        inline void OutWarning(const TLogFunction& logFunc) const { Out(ELogLevel::Warning, logFunc); }
        inline void OutError(const TLogFunction& logFunc) const { Out(ELogLevel::Error, logFunc); }
        inline void OutFatalError(const TLogFunction& logFunc) const { Out(ELogLevel::FatalError, logFunc); }

        inline void Out(const char* text) const
        {
            Out([text](auto& ls) { ls << text; });
        }

        inline void Out(ELogLevel level, const char* text) const
        {
            Out(level, [text](auto& ls) { ls << text; });
        }

        inline void OutWarning(const char* text) const
        {
            OutWarning([text](auto& ls) { ls << text; });
        }

        inline void OutError(const char* text) const
        {
            OutError([text](auto& ls) { ls << text; });
        }

        inline void OutFatalError(const char* text) const
        {
            OutFatalError([text](auto& ls) { ls << text; });
        }
    };

private:
    static Logger* instance;

    MultiPoolAllocator allocator;
    TaskHandle taskHandle;
    std::atomic<bool> hasInput;
    std::atomic<bool> needFlush;

    TString logPath;
    TLogBuffer inputBuffer;
    TLogBuffer swapBuffer;
    TTextBuffer textBuffer;
    TOutputFuncs flushFuncs;
    TFilters filters;

    std::ofstream outFileStream;
    std::thread::id threadID;

    std::mutex filterLock;
    std::mutex inputLock;
    
public:
    static Logger& Get();
    static SimpleLogger Get(StaticString category, ELogLevel level = ELogLevel::Info);

public:
    Logger(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator= (const Logger&) = delete;
    Logger& operator= (Logger&&) = delete;

public:
    Logger(Engine& engine, const char* path, const char* filename);
    ~Logger();
    
    StaticString GetName() const;
    void StartTask(TaskSystem& taskSys);
    void StopTask(TaskSystem& taskSys);

    void SetFilter(StaticString category, TLogFilter filter);
    void AddLog(StaticString category, ELogLevel level, const TLogFunction& logFunc);
    void Flush();

#ifdef PROFILE_ENABLED
    void ReportMemoryConfiguration();
#endif // PROFILE_ENABLED

private:
    void ProcessBuffer();
    void FlushBuffer(const TTextBuffer& buffer);
    
    void WriteLog(const TTextBuffer& buffer);
    void PrintStdIO(const TTextBuffer& buffer) const;
};

using TLog = Logger::SimpleLogger;
using LogStream = Logger::TLogStream;
} // HE
