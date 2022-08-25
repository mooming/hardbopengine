// Created by mooming.go@gmail.com 2022

#include "Logger.h"

#include "LogUtil.h"
#include "Config/EngineConfig.h"
#include "Memory/AllocatorScope.h"
#include "OSAL/Intrinsic.h"
#include "String/StringUtil.h"
#include "System/Debug.h"
#include <iostream>


namespace HE
{

Logger* Logger::instance = nullptr;

Logger::SimpleLogger::SimpleLogger(StaticString category, ELogLevel level)
    : category(category)
    , level(level)
{
}

void Logger::SimpleLogger::Out(TLogFunction logFunc)
{
    Assert(instance != nullptr);
    instance->AddLog(category, level, logFunc);
}

void Logger::SimpleLogger::Out(ELogLevel inLevel, TLogFunction logFunc)
{
    Assert(instance != nullptr);
    instance->AddLog(category, inLevel, logFunc);
}

Logger& Logger::Get()
{
    FatalAssert(instance != nullptr);
    
    return *instance;
}

Logger::SimpleLogger Logger::Get(StaticString category, ELogLevel level)
{
    SimpleLogger log(category, level);
    return log;
}

Logger::Logger(const char* path, const TPathStr& filename, int numRolling)
    : isRunning(false)
    , disableLogPrint(false)
    , startTime(std::chrono::steady_clock::now())
    , logPath(path)
    , allocator("LoggerMemoryPool"
        , Config::LogMemoryBlockSize
        , Config::LogNumMemoryBlocks)
{
    instance = this;

    AllocatorScope scope(allocator);
    
    auto endChar = *(logPath.end());
    
    {
        auto predicate = [](auto item) { return item == '\\'; };
        std::replace_if(logPath.begin(), logPath.end(), predicate, '/');
    }
    
    if (endChar == '/')
    {
        logPath.reserve(logPath.size() + filename.size());
    }
    else
    {
        logPath.reserve(logPath.size() + filename.size() + 1);
        logPath.push_back('/');
    }
    
    logPath.append(filename);
    logPath.shrink_to_fit();
    
    outFileStream.open(logPath.c_str());
    
    ResetOutputBuffer(Config::LogBufferSize);
    
    flushFuncs.reserve(2);
    flushFuncs.push_back([this](const TTextBuffer& buffer) { WriteLog(buffer); });
    flushFuncs.push_back([this](const TTextBuffer& buffer) { PrintStdIO(buffer); });
    
    baseFilter = [](ELogLevel level) -> bool
    {
        return level >= ELogLevel::Info;
    };
}

Logger::~Logger()
{
    Stop();
    logThread.join();
}

StaticString Logger::GetName() const
{
    static auto className = StringUtil::PrettyFunctionToCompactClassName(__PRETTY_FUNCTION__);
    return className;
}

void Logger::AddLog(StaticString category, ELogLevel level, TLogFunction logFunc)
{
#ifndef LOG_ENABLED
    return;
#endif // LOG_ENABLED
    
    if (unlikely(logFunc == nullptr))
        return;

    Assert(filters.size() == 0);

    auto found = filters.find(category);
    auto& filter = found == filters.end()
        ? baseFilter : filters[category];
    
    if (filter != nullptr && !filter(level))
        return;

    TLogStream ls;
    logFunc(ls);

    {
        std::lock_guard lock(inputLock);
        inputBuffer.emplace_back(level, category, ls.str());
    }
    
    cv.notify_one();
}

void Logger::Start()
{
    logThread = std::thread(&Logger::Run, this);
}

void Logger::Stop()
{
    isRunning = false;
    
    AddLog(GetName(), ELogLevel::Info, [](auto& logStream)
    {
        logStream << "Logger shall be terminated.";
    });
    
    cv.notify_all();
}

void Logger::SetFilter(StaticString category, TLogFilter filter)
{
    filters[category] = filter;
}

void Logger::FlushInputBuffers()
{
    std::lock_guard lockInput(inputLock);
    if (inputBuffer.size() <= 0)
        return;
    
    cv.notify_one();
}

void Logger::Run()
{
    AllocatorScope scope(allocator);

    using namespace std;
    
    isRunning = true;
    startTime = chrono::steady_clock::now();
    
    AddLog(GetName(), ELogLevel::Info, [](auto& logStream)
    {
        logStream << "Logger started.";
    });
    
    auto ProcessBuffer = [this]()
    {
        AllocatorScope scope(allocator);

        static TLogBuffer tmpBuffer;
        
        {
            std::lock_guard lockInput(inputLock);
            std::swap(inputBuffer, tmpBuffer);
        }
        
        if (!tmpBuffer.empty())
        {
            static TTextBuffer textBuffer;
            textBuffer.reserve(tmpBuffer.size());
            
            for (auto& log : tmpBuffer)
            {
                auto timeStampStr = LogUtil::GetTimeStampString(startTime, log.timeStamp);
                auto levelStr = LogUtil::GetLogLevelString(log.level);
                
                TString text;
                text.reserve(1024);
                text.push_back('[');
                text.append(timeStampStr);
                text.append("][");
                text.append(log.category.c_str());
                text.append("][");
                text.append(levelStr);
                text.append("] ");
                text.append(log.text);
                
                textBuffer.emplace_back(std::move(text));
            }
            
            tmpBuffer.clear();
            
            Flush(textBuffer);
            textBuffer.clear();
        }
    };

    while (isRunning)
    {
        ProcessBuffer();
        
        std::unique_lock lock(cvLock);
        cv.wait(lock);
    }
    
    AddLog(GetName(), ELogLevel::Info, [](auto& logStream)
    {
        logStream << "Logger is terminated.";
    });
    
    ProcessBuffer();
}

void Logger::ResetOutputBuffer(size_t size)
{
    AllocatorScope scope(allocator);
    
    outputBuffer.clear();
    outputBuffer.shrink_to_fit();
    outputBuffer.reserve(size);
    outputBuffer.resize(size);
}

void Logger::Flush(const TTextBuffer& buffer)
{
    for (auto func : flushFuncs)
    {
        Assert(func != nullptr);
        func(buffer);
    }
}

void Logger::WriteLog(const TTextBuffer& buffer)
{
    using namespace std;
    
    auto& ofs = outFileStream;

    for (auto& logText : buffer)
    {
        ofs << logText << endl;
    }
    
    ofs.flush();
}

void Logger::PrintStdIO(const TTextBuffer& buffer) const
{
    using namespace std;
    
    if (disableLogPrint)
        return;

    for (auto& logText : buffer)
    {
        cout << logText << endl;
    }
}

} // HE
