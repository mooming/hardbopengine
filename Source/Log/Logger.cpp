// Created by mooming.go@gmail.com 2022

#include "Logger.h"

#include "LogUtil.h"
#include "Config/EngineConfig.h"
#include "Memory/AllocatorScope.h"
#include "Memory/InlinePoolAllocator.h"
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

void Logger::SimpleLogger::Out(TLogFunction logFunc) const
{
    Assert(instance != nullptr);
    instance->AddLog(category, level, logFunc);
}

void Logger::SimpleLogger::Out(ELogLevel inLevel, TLogFunction logFunc) const
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

Logger::Logger(const char* path, const char* filename, int numRolling)
    : isRunning(false)
    , hasInput(false)
    , needFlush(false)
    , startTime(std::chrono::steady_clock::now())
    , allocator("LoggerMemoryPool"
        , Config::LogMemoryBlockSize
        , Config::LogNumMemoryBlocks)
    , logPath(path)
{
    instance = this;

    AllocatorScope scope(allocator);

    auto endChar = logPath[logPath.size() - 1];
    
    {
        auto predicate = [](auto item) { return item == '\\'; };
        std::replace_if(logPath.begin(), logPath.end(), predicate, '/');
    }

    auto fileNameSize = StringUtil::StrLen(filename, Config::MaxPathLength);
    if (endChar == '/')
    {
        logPath.reserve(logPath.size() + fileNameSize);
    }
    else
    {
        logPath.reserve(logPath.size() + fileNameSize + 1);
        logPath.push_back('/');
    }
    
    logPath.append(filename);
    logPath.shrink_to_fit();
    
    outFileStream.open(logPath.c_str());
    
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

    AllocatorScope scope(InvalidAllocatorID);
    
    if (unlikely(logFunc == nullptr))
    {
        AddLog(GetName(), ELogLevel::Warning, [](auto& ls)
        {
            ls << "Null log function!";
        });
        
        return;
    }

    {
        std::lock_guard lock(filterLock);
        auto found = filters.find(category);
        auto& filter = found == filters.end()
            ? baseFilter : filters[category];

        if (filter != nullptr && !filter(level))
            return;
    }

    TLogStream ls;
    logFunc(ls);
    
    if (unlikely(level >= ELogLevel::Error && std::this_thread::get_id() == logThread.get_id()) )
    {
        TTextBuffer tmpTextBuffer;
        tmpTextBuffer.reserve(1);

        using namespace std;
        auto timeStampStr = LogUtil::GetTimeStampString(startTime, chrono::steady_clock::now());
        auto levelStr = LogUtil::GetLogLevelString(level);
        
        InlineStringBuilder<Config::LogLineSize + 64> text;
        
        text << '[' << timeStampStr << "][" << category << "][" << levelStr
            << "] " << ls.c_str();
        
        tmpTextBuffer.emplace_back(text.c_str());
        
        Flush(tmpTextBuffer);
        tmpTextBuffer.clear();
        
        if (unlikely(level >= ELogLevel::FatalError))
        {
            debugBreak();
        }
        
        return;
    }
    
    {
        std::lock_guard lock(inputLock);
        inputBuffer.emplace_back(level, category, ls.c_str());
        hasInput = true;
        needFlush = true;
    }
    
    if (unlikely(level >= ELogLevel::FatalError))
    {
        debugBreak();
        
        Stop();
        logThread.join();

        Assert(false);
        return;
    }
    
    {
        std::lock_guard lock(cvLock);
        cv.notify_one();
    }
    
}

void Logger::Start()
{
    logThread = std::thread(&Logger::Run, this);
}

void Logger::Stop()
{
    if (isRunning)
    {
        std::lock_guard lock(cvLock);
        isRunning = false;
        cv.notify_all();
    }

    if (logThread.joinable())
    {
        logThread.join();
    }
}

void Logger::SetFilter(StaticString category, TLogFilter filter)
{
    std::lock_guard lock(filterLock);
    filters[category] = filter;
}

void Logger::Flush()
{
    if (std::this_thread::get_id() == logThread.get_id())
        return;

    while(needFlush)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void Logger::Run()
{
    using namespace std;
    
    AllocatorScope scope(allocator);

    isRunning = true;
    startTime = chrono::steady_clock::now();
    
    AddLog(GetName(), ELogLevel::Info, [](auto& logStream)
    {
        logStream << "Logger started.";
    });
    
    auto ProcessBuffer = [this]()
    {
        {
            std::lock_guard lockInput(inputLock);
            std::swap(inputBuffer, swapBuffer);
            hasInput = false;
        }
        
        if (!swapBuffer.empty())
        {
            textBuffer.reserve(swapBuffer.size());

            for (auto& log : swapBuffer)
            {
                auto timeStampStr = LogUtil::GetTimeStampString(startTime, log.timeStamp);
                auto levelStr = LogUtil::GetLogLevelString(log.level);

                using namespace HSTL;
                HInlineString<Config::LogBufferSize> text;
                text.reserve(Config::LogLineSize);

                text.push_back('[');
                text.append(timeStampStr);
                text.append("][");
                text.append(log.category);
                text.append("][");
                text.append(levelStr);
                text.append("] ");
                text.append(log.text);

                textBuffer.emplace_back(text.c_str());
            }
            
            swapBuffer.clear();
            
            Flush(textBuffer);
            textBuffer.clear();
        }
    };

    {
        auto waitPeriod = std::chrono::milliseconds(16);
        while (isRunning)
        {
            ProcessBuffer();
            needFlush = false;

            if (!hasInput)
            {
                std::unique_lock lock(cvLock);
                cv.wait_for(lock, waitPeriod);
            }
        }
    }

    AddLog(GetName(), ELogLevel::Info, [](auto& ls)
    {
        ls << "Logger has been terminated." << hendl;
    });
    
    ProcessBuffer();
    
    outFileStream.flush();
    outFileStream.close();
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
    auto& ofs = outFileStream;

    for (auto& logText : buffer)
    {
        ofs << logText << std::endl;
    }
    
    ofs.flush();
}

void Logger::PrintStdIO(const TTextBuffer& buffer) const
{
    for (auto& logText : buffer)
    {
        std::cout << logText << std::endl;
    }
}

} // HE
