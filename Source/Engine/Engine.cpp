// Created by mooming.go@gmail.com

#include "Engine.h"

#include "../Config/ConfigParam.h"
#include "../Config/ConfigSystem.h"
#include "../HSTL/HString.h"
#include "../String/InlineStringBuilder.h"
#include "../String/StaticStringTable.h"
#include "../System/Debug.h"
#include "../System/ScopedLock.h"
#include <csignal>
#include <iostream>

namespace
{
void SignalHandler(int sigNum)
{
    using namespace HE;

    auto& engine = Engine::Get();
    engine.GetLogger().StopTask(engine.GetTaskSystem());
    engine.LogError(
        [sigNum](auto& ls) { ls << "signal(" << sigNum << ") received."; });
    engine.LogError(
        [](auto& ls) { ls << "The application shall be terminated."; });
    engine.LogError([](auto& ls) {
        ls << "Thank you for playing. Have a great day! :)" << std::endl;
    });

    engine.CloseLog();

    exit(sigNum);
}

} // namespace

namespace HE
{
static Engine* engineInstance = nullptr;

Engine& Engine::Get()
{
    Assert(engineInstance != nullptr);
    return *engineInstance;
}

Engine::PreEngineInit::PreEngineInit(Engine* engine)
{
    engineInstance = engine;
}

Engine::Engine()
    : preEngineInit(this),
      isRunning(false),
      isMemoryManagerReady(false),
      isSystemStatisticsReady(false),
      isLoggerReady(false),
      isTaskSystemReady(false),
      isResourceManagerReady(false),
      logFile("helowlevel.log"),
      memoryManager(*this),
      statistics(*this),
      logger(*this, "./", "hardbop.log")
{
    std::signal(SIGABRT, SignalHandler);
#ifdef SIGBUS
    std::signal(SIGBUS, SignalHandler);
#endif // SIGBUS
    std::signal(SIGFPE, SignalHandler);
    std::signal(SIGILL, SignalHandler);
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGSEGV, SignalHandler);
    std::signal(SIGTERM, SignalHandler);
}

Engine::~Engine()
{
    CloseLog();
}

void Engine::Initialize(int argc, const char* argv[])
{
    taskSystem.Initialize();
    isTaskSystemReady = true;

    logger.StartTask(taskSystem);
    isLoggerReady = true;

    auto log = Logger::Get(GetClassName());

    log.Out("Command Line Arguments");

    for (int i = 0; i < argc; ++i)
    {
        log.Out([i, argv](auto& ls) { ls << i << " : " << argv[i]; });
    }

    log.Out("Engine has been initialized.");

    PostInitialize();
}

void Engine::Run()
{
    isRunning = true;

    {
        MultiPoolAllocator allocator("Main");
        AllocatorScope scope(allocator);

        while (likely(isRunning))
        {
            statistics.IncFrameCount();
            statistics.UpdateCurrentTime();

            const auto deltaTime = statistics.GetDeltaTime();
            PreUpdate(deltaTime);
            Update(deltaTime);
            PostUpdate(deltaTime);
        }
    }

    {
        auto& configSys = ConfigSystem::Get();

#ifdef __DEBUG__
//        const auto logLevel = static_cast<uint8_t>(ELogLevel::Verbose);
//        configSys.SetByte("Log.Engine", logLevel);
//        configSys.SetByte("Log.Level", logLevel);
#endif // __DEBUG__

        auto& staticStrTable = StaticStringTable::GetInstance();
        staticStrTable.PrintStringTable();
        configSys.PrintAllParameters();
        statistics.Print();
        statistics.PrintAllocatorProfiles();
    }

    PreShutdown();

    auto log = Logger::Get(GetClassName(), ELogLevel::Info);
    log.Out("Shutting down...");

    logger.StopTask(taskSystem);
    taskSystem.Shutdown();
}

void Engine::Stop()
{
    isRunning = false;
}

StaticString Engine::GetClassName() const
{
    static StaticString name("Engine");
    return name;
}

void Engine::Log(ELogLevel level, TLogFunc func)
{
#ifdef ENGINE_LOG_ENABLED
    static TAtomicConfigParam<uint8_t> CPEngineLogLevel("Log.Engine",
        "The Engine Log Level", static_cast<uint8_t>(Config::EngineLogLevel));

    static TAtomicConfigParam<uint8_t> CPEnginePrintLogLevel("Log.Engine.Print",
        "The Engine Log Level for Standard IO",
        static_cast<uint8_t>(Config::EngineLogLevelPrint));

    auto levelAsValue = static_cast<uint8_t>(level);
    if (levelAsValue < CPEngineLogLevel.Get())
    {
        return;
    }

    using namespace std;

    const auto diff = chrono::steady_clock::now() - statistics.GetStartTime();
    auto hours = chrono::duration_cast<chrono::hours>(diff);
    auto minutes = chrono::duration_cast<chrono::minutes>(diff);
    auto seconds = chrono::duration_cast<chrono::seconds>(diff);
    auto milliSeconds = chrono::duration_cast<chrono::milliseconds>(diff);

    auto intHours = hours.count();
    auto intMins = minutes.count() % 60;
    auto intSecs = seconds.count() % 60;
    auto intMSecs = milliSeconds.count() % 1000;

    statistics.IncEngineLogCount();

    {
        std::lock_guard lock(logLock);

        if (levelAsValue >= CPEnginePrintLogLevel.Get())
        {
            std::stringstream ss;
            ss << '[' << intHours << ':' << intMins << ':' << intSecs << '.'
               << intMSecs << "] ";

            func(ss);

            ConsoleOutLn(ss.str().c_str());
            ss.str("");
        }

        Assert(logFile.is_open());

        logFile << '[' << intHours << ':' << intMins << ':' << intSecs << '.'
                << intMSecs << "] ";

        func(logFile);

        logFile << endl;
    }
#endif // ENGINE_LOG_ENABLED
}

void Engine::CloseLog()
{
    if (!logFile.is_open())
    {
        return;
    }

    logFile.flush();
}

void Engine::FlushLog()
{
    logger.Flush();

    if (!logFile.is_open())
    {
        return;
    }

    logFile.flush();
}

void Engine::ConsoleOutLn(const char* str)
{
    std::lock_guard lock(consoleOutLock);
    std::cout << str << std::endl;
}

void Engine::PostInitialize()
{
    using namespace StringUtil;
    auto log = Logger::Get(ToCompactMethodName(__PRETTY_FUNCTION__));

    log.Out("Engine PostInitialize [Start]");

    memoryManager.PostEngineInit();

    log.Out("Engine PostInitialize [Done]");
}

void Engine::PreUpdate(float deltaTime)
{
    Log(ELogLevel::Info, [deltaTime](auto& ls) {
        ls << "PreUpdate: deltaTime = " << deltaTime;
    });

    auto& mainTaskStream = taskSystem.GetMainTaskStream();
    mainTaskStream.Flush();
}

void Engine::Update(float deltaTime)
{
    Log(ELogLevel::Info,
        [deltaTime](auto& ls) { ls << "Update: deltaTime = " << deltaTime; });

    auto& mainTaskStream = taskSystem.GetMainTaskStream();
    mainTaskStream.Flush();
}

void Engine::PostUpdate(float deltaTime)
{
    Log(ELogLevel::Info, [deltaTime](auto& ls) {
        ls << "PostUpdate: deltaTime = " << deltaTime;
    });

    auto& mainTaskStream = taskSystem.GetMainTaskStream();
    mainTaskStream.Flush();

    taskSystem.PostUpdate();
}

void Engine::PreShutdown()
{
    using namespace StringUtil;
    auto log = Logger::Get(ToCompactMethodName(__PRETTY_FUNCTION__));

    log.Out("Engine PreShutdown [Start]");

#ifdef PROFILE_ENABLED
    logger.ReportMemoryConfiguration();
#endif // PROFILE_ENABLED

    memoryManager.PreEngineShutdown();

    log.Out("Engine PreShutdown [Done]");
}

} // namespace HE
