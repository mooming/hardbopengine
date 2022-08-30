// Created by mooming.go@gmail.com, 2017

#include "Test/TestCase.h"

#include "Log/Logger.h"
#include <iostream>
#include <exception>


using namespace HE;

TestCase::TestCase(const char* title)
    : title(title)
    , isDone(false)
    , isSuccess(false)
{
}

void TestCase::Start()
{
    TLog log(GetName(), ELogLevel::Info);
    
    try
    {
        log.Out([title = GetName()](auto& ls)
        {
            ls << "START ====================";
        });
        
        isSuccess = DoTest();
        isDone = true;
    }
    catch (std::exception& e)
    {
        log.OutError([title = GetName(), &e](auto& ls)
        {
            ls << e.what() << std::endl;
        });
        
        isSuccess = false;
    }

    Report();
}

void TestCase::Report()
{
    TLog log(GetName(), ELogLevel::Info);
    
    if (isSuccess)
    {
        log.Out([title = GetName()](auto& ls)
        {
            ls << "Result: [SUCCESS]" << std::endl;
        });
    }
    else
    {
        log.OutError([](auto& ls)
        {
            ls << "Result [FAIL]" << std::endl;
        });
    }
}
