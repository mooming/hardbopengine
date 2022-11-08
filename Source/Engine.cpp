// Created by mooming.go@gmail.com, 2022

#include "Engine.h"

#include "Config/ConfigSystem.h"
#include "Config/ConfigParam.h"
#include "HSTL/HString.h"
#include "String/StaticStringTable.h"
#include "System/Debug.h"
#include "System/ScopedLock.h"
#include "System/Time.h"
#include <csignal>
#include <iostream>


namespace
{

void SignalHandler(int sigNum)
{
    using namespace HE;
    
    auto& engine = Engine::Get();
    engine.GetLogger().StopTask(engine.GetTaskSystem());
    
    engine.LogError([sigNum](auto& ls)
    {
        ls << "signal(" << sigNum << ") received.";
    });
    
    engine.LogError([](auto& ls)
    {
        ls << "The application shall be terminated.";
    });
    
    engine.LogError([](auto& ls)
    {
        ls << "Thank you for playing. Have a great day! :)" << std::endl;
    });
    
    engine.CloseLog();

    exit(sigNum);
}

} // Anonymous

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
    : preEngineInit(this)
    , logFile("helowlevel.log")
    , logger("./", "hardbop.log", 5)
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
    logger.StartTask(taskSystem);

    auto log = Logger::Get(GetName(), ELogLevel::Info);
    
    log.Out("Command Line Arguments");

    for (int i = 0; i < argc; ++i)
    {
        log.Out([i, argv](auto& ls)
        {
            ls << i << " : " << argv[i];
        });
    }

    log.Out("Engine has been initialized.");
}

void Engine::Run()
{
    isRunning = true;

    while(likely(isRunning))
    {
        statistics.IncFrameCount();
        statistics.UpdateCurrentTime();
        auto deltaTime = statistics.GetDeltaTime();
        
        PreUpdate(deltaTime);
        Update(deltaTime);
        PostUpdate(deltaTime);

        Stop();
    }

    auto& configSys = ConfigSystem::Get();
    
#ifdef __DEBUG__
    const auto logLevel = static_cast<uint8_t>(ELogLevel::Verbose);
    configSys.SetByte("Log.Engine", logLevel);
    configSys.SetByte("Log.Level", logLevel);
#endif // __DEBUG__

    auto& staticStrTable = StaticStringTable::GetInstance();
    staticStrTable.PrintStringTable();
    
    configSys.PrintAllParameters();
    statistics.Print();
    
#ifdef PROFILE_ENABLED
    statistics.PrintAllocatorProfiles();
#endif // PROFILE_ENABLED

    auto log = Logger::Get(GetName(), ELogLevel::Info);
    log.Out("Shutting down...");
    
    logger.StopTask(taskSystem);
    taskSystem.Shutdown();
}

void Engine::Stop()
{
    isRunning = false;
}

StaticString Engine::GetName() const
{
    static StaticString name("HEngine");
    return name;
}

void Engine::Log(ELogLevel level, TLogFunc func)
{
#ifdef ENGINE_LOG_ENABLED
    static TAtomicConfigParam<uint8_t> CPEngineLogLevel("Log.Engine"
       , "The Engine Log Level"
       , static_cast<uint8_t>(Config::EngineLogLevel));

    static TAtomicConfigParam<uint8_t> CPEnginePrintLogLevel("Log.Engine.Print"
       , "The Engine Log Level for Standard IO"
       , static_cast<uint8_t>(Config::EngineLogLevelPrint));

    auto levelAsValue = static_cast<uint8_t>(level);
    if (levelAsValue < CPEngineLogLevel.Get())
        return;

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
        ScopedLock lock(logLock);

        if (levelAsValue >= CPEnginePrintLogLevel.Get())
        {
            cerr << '[' << intHours << ':' << intMins << ':' << intSecs
                << '.' << intMSecs << "] ";
            func(cerr);
            cerr << endl;
        }

        Assert(logFile.is_open());
        logFile << '[' << intHours << ':' << intMins << ':' << intSecs
            << '.' << intMSecs << "] ";

        func(logFile);

        logFile << endl;
    }
#endif // ENGINE_LOG_ENABLED
}

void Engine::CloseLog()
{
    if (!logFile.is_open())
        return;
    
    logFile.flush();
}

void Engine::FlushLog()
{
    logger.Flush();

    if (!logFile.is_open())
        return;

    logFile.flush();
}

void Engine::PreUpdate(float deltaTime)
{
    Log(ELogLevel::Info, [deltaTime](auto& ls)
    {
        ls << "PreUpdate: deltaTime = " << deltaTime;
    });

    auto& mainTaskStream = taskSystem.GetMainTaskStream();
    mainTaskStream.Flush();
}

void Engine::Update(float deltaTime)
{
    Log(ELogLevel::Info, [deltaTime](auto& ls)
    {
        ls << "Update: deltaTime = " << deltaTime;
    });

    auto& mainTaskStream = taskSystem.GetMainTaskStream();
    mainTaskStream.Flush();
}

void Engine::PostUpdate(float deltaTime)
{
    Log(ELogLevel::Info, [deltaTime](auto& ls)
    {
        ls << "PostUpdate: deltaTime = " << deltaTime;
    });

    auto& mainTaskStream = taskSystem.GetMainTaskStream();
    mainTaskStream.Flush();

    taskSystem.PostUpdate();
}

} // HE
