// Created by mooming.go@gmail.com, 2022

#include "Engine.h"

#include "HSTL/HString.h"
#include "System/Debug.h"
#include <iostream>


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
}

Engine::~Engine()
{
    logFile.flush();
    logFile.close();
}

void Engine::Initialize(int argc, const char* argv[])
{
    logger.Start();

    auto log = Logger::Get(GetName(), ELogLevel::Info);
    
    log.Out([](auto& logStream)
    {
        logStream << "Command Line Arguments";
    });

    for (int i = 0; i < argc; ++i)
    {
        log.Out([i, argv](auto& logStream)
        {
            logStream << i << " : " << argv[i];
        });
    }

    log.Out([](auto& logStream)
    {
        logStream << "Engine has been initialized.";
    });
}

void Engine::Run()
{
    staticStringTable.PrintStringTable();
    auto log = Logger::Get(GetName(), ELogLevel::Info);
    log.Out([](auto& logStream)
    {
        logStream << "Shutting down...";
    });
    
    logger.Stop();
}

StaticString Engine::GetName() const
{
    static StaticString name("HEngine");
    return name;
}

void Engine::Log(ELogLevel level, TLogFunc func)
{
#ifdef ENGINE_LOG_ENABLED
    if (static_cast<uint8_t>(level) < Config::EngineLogLevel)
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
    
    cerr << "[EngineLog][" << intHours << ':' << intMins << ':' << intSecs
        << '.' << intMSecs << ']';
    func(cerr);
    cerr << endl;
    
    logFile << '[' << intHours << ':' << intMins << ':' << intSecs
        << '.' << intMSecs << ']';
    func(logFile);
    logFile << endl;
#endif // ENGINE_LOG_ENABLED
}

} // HE
