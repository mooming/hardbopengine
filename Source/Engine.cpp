// Created by mooming.go@gmail.com, 2022

#include "Engine.h"

#include "HSTL/HString.h"
#include "System/Debug.h"
#include <csignal>
#include <iostream>


namespace
{

void SignalHandler(int sigNum)
{
    using namespace HE;
    
    auto& engine = Engine::Get();
    engine.GetLogger().Stop();
    
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
    , startTime(std::chrono::steady_clock::now())
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
    logger.Start();

    auto log = Logger::Get(GetName(), ELogLevel::Info);
    
    log.Out([](auto& ls)
    {
        ls << "Command Line Arguments";
    });

    for (int i = 0; i < argc; ++i)
    {
        log.Out([i, argv](auto& ls)
        {
            ls << i << " : " << argv[i];
        });
    }

    taskSystem.Initialize();

    log.Out([](auto& ls)
    {
        ls << "Engine has been initialized.";
    });
}

void Engine::Run()
{
    {
        PreUpdate();
        Update();
        PostUpdate();
    }

    taskSystem.Shutdown();

    staticStringTable.PrintStringTable();
    auto log = Logger::Get(GetName(), ELogLevel::Info);
    log.Out([](auto& logStream)
    {
        logStream << "Shutting down...";
    });
    
    logger.Stop();
}

void Engine::FlushLog()
{
    logger.Flush();

    if (!logFile.is_open())
        return;

    logFile.flush();
}

StaticString Engine::GetName() const
{
    static StaticString name("HEngine");
    return name;
}

void Engine::Log(ELogLevel level, TLogFunc func)
{
#ifdef ENGINE_LOG_ENABLED
    auto levelAsValue = static_cast<uint8_t>(level);
    if (levelAsValue < Config::EngineLogLevel)
        return;

    using namespace std;
    
    const auto diff = chrono::steady_clock::now() - startTime;
    auto hours = chrono::duration_cast<chrono::hours>(diff);
    auto minutes = chrono::duration_cast<chrono::minutes>(diff);
    auto seconds = chrono::duration_cast<chrono::seconds>(diff);
    auto milliSeconds = chrono::duration_cast<chrono::milliseconds>(diff);
    
    int intHours = hours.count();
    int intMins = minutes.count() % 60;
    int intSecs = seconds.count() % 60;
    int intMSecs = milliSeconds.count() % 1000;
    
    lock_guard lock(logLock);

    if (levelAsValue >= Config::EngineLogLevelPrint)
    {
        cerr << '[' << intHours << ':' << intMins << ':' << intSecs
            << '.' << intMSecs << "][EngineLog] ";
        func(cerr);
        cerr << endl;
    }

    Assert(logFile.is_open());
    logFile << '[' << intHours << ':' << intMins << ':' << intSecs
        << '.' << intMSecs << "] ";
    
    func(logFile);
    logFile << endl;
#endif // ENGINE_LOG_ENABLED
}

void Engine::CloseLog()
{
    if (!logFile.is_open())
        return;
    
    logFile.flush();
    logFile.close();
}

void Engine::PreUpdate()
{

}

void Engine::Update()
{

}

void Engine::PostUpdate()
{
    taskSystem.PostUpdate();
}

} // HE
