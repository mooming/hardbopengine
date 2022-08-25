// Created by mooming.go@gmail.com, 2022

#include "Engine.h"
#include "HSTL/HString.h"
#include <iostream>


namespace HE
{

Engine::Engine()
    : logger("./", "Engine.log", 5)
{
}

Engine::~Engine()
{
    staticStringTable.PrintStringTable();
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
    auto log = Logger::Get(GetName(), ELogLevel::Info);
    log.Out([](auto& logStream)
    {
        logStream << "Engine shall be terminated.";
    });
}

StaticString Engine::GetName() const
{
    static StaticString name("HEngine");
    return name;
}

} // HE
