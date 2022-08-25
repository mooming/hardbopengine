// Created by mooming.go@gmail.com 2022

#pragma once

#include "LogLevel.h"
#include "LogLine.h"
#include "HSTL/HString.h"
#include "HSTL/HStringStream.h"
#include "HSTL/HVector.h"
#include "HSTL/HUnorderedMap.h"
#include "Memory/PoolAllocator.h"
#include "String/StaticString.h"
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <functional>
#include <mutex>
#include <thread>


namespace HE
{

class Logger final
{
private:
    using TString = HSTL::HString;
    using TPathStr = HSTL::HPathString;
    using TLogBuffer = HSTL::HVector<LogLine>;
    using TTextBuffer = HSTL::HVector<TString>;
    using TTimePoint = std::chrono::time_point<std::chrono::steady_clock>;
    using TLogStream = HSTL::HStringStream;
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

        SimpleLogger(StaticString category, ELogLevel level);
        void Out(TLogFunction logFunc);
        void Out(ELogLevel level, TLogFunction logFunc);
    };

private:
    static Logger* instance;
    
    bool isRunning;
    bool disableLogPrint;
    TTimePoint startTime;
    
    TString logPath;
    TLogBuffer inputBuffer;
    TLogBuffer outputBuffer;
    TOutputFuncs flushFuncs;
    TFilters filters;
    TLogFilter baseFilter;
    
    PoolAllocator allocator;
    
    std::ofstream outFileStream;
    std::thread logThread;
    std::mutex inputLock;
    std::mutex cvLock;
    std::condition_variable cv;
    
public:
    static Logger& Get();
    static SimpleLogger Get(StaticString category, ELogLevel level);
    Logger(const char* path, const TPathStr& filename, int numRolling);
    ~Logger();
    
    StaticString GetName() const;
    void AddLog(StaticString category, ELogLevel level, TLogFunction logFunc);
    void Start();
    void Stop();
    
    void SetFilter(StaticString category, TLogFilter filter);
    void FlushInputBuffers();
    
private:
    void Run();
    void ResetOutputBuffer(size_t size);
    void Flush(const TTextBuffer& buffer);
    
    void WriteLog(const TTextBuffer& buffer);
    void PrintStdIO(const TTextBuffer& buffer) const;
};

using TLog = Logger::SimpleLogger;
} // HE
