// Created by mooming.go@gmail.com 2022

#include "Logger.h"

#include "Engine.h"
#include "LogUtil.h"
#include "Config/BuildConfig.h"
#include "Config/EngineSettings.h"
#include "Config/ConfigParam.h"
#include "Memory/AllocatorScope.h"
#include "Memory/InlinePoolAllocator.h"
#include "OSAL/Intrinsic.h"
#include "String/StringUtil.h"
#include "System/Debug.h"
#include "System/ScopedLock.h"
#include "System/TaskSystem.h"
#include <iostream>
#include <memory>


namespace HE
{

namespace
{

StaticString GetLoggerTaskName()
{
    static const StaticString name("LoggerTask");
    return name;
}

#ifdef LOG_FORCE_IMMEDIATE
void ImmediateLog(ELogLevel level, StaticString category, const char* logStr)
{
    AllocatorScope scope(MemoryManager::SystemAllocatorID);

    using namespace std;
    auto timeStampStr = LogUtil::GetTimeStampString();
    auto levelStr = LogUtil::GetLogLevelString(level);

    cout << '[' << timeStampStr << "][" << std::this_thread::get_id() << "]["
        << category << "][" << levelStr << "] " << logStr << endl;
}
#endif // LOG_FORCE_IMMEDIATE

} // anonymous namespace

Logger* Logger::instance = nullptr;

Logger::SimpleLogger::SimpleLogger(StaticString category, ELogLevel level)
    : category(category)
    , level(level)
{
}

void Logger::SimpleLogger::Out(const TLogFunction& logFunc) const
{
    if (unlikely(instance == nullptr))
    {
        return;
    }

    instance->AddLog(category, level, logFunc);
}

void Logger::SimpleLogger::Out(ELogLevel inLevel, const TLogFunction& logFunc) const
{
    if (unlikely(instance == nullptr))
    {
        return;
    }

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
    : hasInput(false)
    , needFlush(false)
    , allocator("LoggerMemoryPool"
        , Config::LogMemoryBlockSize
        , Config::LogNumMemoryBlocks)
    , logPath(path)
{
    instance = this;
    LogUtil::GetStartTime();

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
}

Logger::~Logger()
{
    ProcessBuffer();

    outFileStream.flush();
    outFileStream.close();
}

StaticString Logger::GetName() const
{
    static auto className = StringUtil::PrettyFunctionToCompactClassName(__PRETTY_FUNCTION__);
    return className;
}

void Logger::StartTask(TaskSystem& taskSys)
{
    AddLog(GetName(), ELogLevel::Info, [](auto& logStream)
    {
        logStream << "Logger started.";
    });

    auto ioTaskIndex = taskSys.GetIOTaskStreamIndex();
    auto func = [this](size_t, size_t)
    {
        ProcessBuffer();
    };

    auto taskName = GetLoggerTaskName();
    taskHandle = taskSys.RegisterTask(ioTaskIndex, taskName, func);

    auto task = taskHandle.GetTask();
    if (unlikely(task == nullptr))
    {
        auto logFunc = [taskName, ioTaskIndex](auto& ls)
        {
            ls << taskName.c_str()
                << " : Failed to register as a task at task index "
                << ioTaskIndex << '.';
        };

        AddLog(GetName(), ELogLevel::FatalError, logFunc);
    }

    auto& ioTaskStream = taskSys.GetIOTaskStream();
    threadID = ioTaskStream.GetThreadID();
}

void Logger::StopTask(TaskSystem& taskSys)
{
    if (!taskHandle.IsValid())
        return;

    taskHandle.Reset();
    threadID = std::thread::id();

    AddLog(GetName(), ELogLevel::Info, [](auto& ls)
    {
        ls << "Logger shall be terminated." << hendl;
    });

    ProcessBuffer();

    outFileStream.flush();
    outFileStream.close();
}


void Logger::AddLog(StaticString category, ELogLevel level
    , const TLogFunction& logFunc)
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

    static TAtomicConfigParam<uint8_t> CPLogLevel("Log.Level"
       , "The Default Log Level"
       , static_cast<uint8_t>(ELogLevel::Info));

    if (level < static_cast<ELogLevel>(CPLogLevel.Get()))
        return;

    {
        ScopedLock lock(filterLock);
        auto found = filters.find(category);
        if (found != filters.end())
        {
            auto& filter = found->second;
            if (filter != nullptr && !filter(level))
                return;
        }
    }

    auto& engine = Engine::Get();
    auto& taskSystem = engine.GetTaskSystem();
    auto threadName = taskSystem.GetCurrentStreamName();

    TLogStream ls;
    logFunc(ls);

#ifdef LOG_FORCE_IMMEDIATE
    ImmediateLog(level, category, ls.c_str());
    return;
#endif // LOG_FORCE_IMMEDIATE

    if (unlikely(level >= ELogLevel::Error && std::this_thread::get_id() == threadID))
    {
        AllocatorScope scope(MemoryManager::SystemAllocatorID);

        static TTextBuffer tmpTextBuffer;
        tmpTextBuffer.reserve(1);

        using namespace std;
        auto timeStampStr = LogUtil::GetTimeStampString();
        auto levelStr = LogUtil::GetLogLevelString(level);

        InlineStringBuilder<Config::LogLineLength * 8> text;
        text << '[' << timeStampStr << "][" << threadName << "]["
            << category << "][" << levelStr << "] " << ls.c_str();
        
        tmpTextBuffer.emplace_back(text.c_str());
        
        FlushBuffer(tmpTextBuffer);
        tmpTextBuffer.clear();
        
        if (unlikely(level >= ELogLevel::FatalError))
        {
            debugBreak();
        }
        
        return;
    }

    size_t bufferSize = 0;

    {
        ScopedLock lock(inputLock);

        if (unlikely(ls.Size() >= Config::LogLineLength))
        {
            inputBuffer.emplace_back(level, threadName, category, true, ls.c_str());
        }
        else
        {
            inputBuffer.emplace_back(level, threadName, category, ls.c_str());
        }

        bufferSize = inputBuffer.size();
        hasInput = true;
        needFlush = true;
    }
    
    if (unlikely(level >= ELogLevel::FatalError))
    {
        debugBreak();
        Flush();

        Assert(false);
        return;
    }

    if (bufferSize >= Config::LogForceFlushThreshold)
    {
        Flush();
    }
}

void Logger::SetFilter(StaticString category, TLogFilter filter)
{
    ScopedLock lock(filterLock);
    filters[category] = filter;
}

void Logger::Flush()
{
    if (std::this_thread::get_id() == threadID)
        return;

    const auto period = std::chrono::milliseconds(10);
    
    while(needFlush)
    {
        std::this_thread::sleep_for(period);
    }
}

void Logger::ProcessBuffer()
{
    if (!hasInput)
        return;

    AllocatorScope scope(allocator);

    {
        ScopedLock lockInput(inputLock);
        std::swap(inputBuffer, swapBuffer);
        hasInput = false;
    }

    if (swapBuffer.empty())
        return;

    bool bNeedIOFlush = false;
    textBuffer.reserve(swapBuffer.size());

    for (auto& log : swapBuffer)
    {
        if (log.level >= ELogLevel::Warning)
            bNeedIOFlush = true;

        auto timeStampStr = LogUtil::GetTimeStampString(log.timeStamp);
        auto levelStr = LogUtil::GetLogLevelString(log.level);

        using namespace HSTL;
        InlineStringBuilder<Config::LogLineLength * 2> text;

        text << '[' <<timeStampStr << "][" << log.threadName << "][";
        text << log.category << "][" << levelStr << "] ";
        text << log.GetText();

        textBuffer.emplace_back(text.c_str());
    }

    swapBuffer.clear();

    FlushBuffer(textBuffer);
    textBuffer.clear();

    if (bNeedIOFlush)
    {
        outFileStream.flush();
    }

    needFlush = false;
}

void Logger::FlushBuffer(const TTextBuffer& buffer)
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
