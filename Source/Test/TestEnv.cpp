// Created by mooming.go@gmail.com, 2017

#include "TestCollection.h"
#include "TestEnv.h"
#include "Log/Logger.h"
#include "System/Debug.h"
#include "System/Exception.h"
#include <iostream>
#include <sstream>


using namespace HE;

TestEnv& TestEnv::GetEnv()
{
    static TestEnv instance;
    return instance;
}

void TestEnv::Start()
{
    invalidTests.clear();
    failedTests.clear();
    
    for (auto& testCase : tests)
    {
        Assert(testCase != nullptr);
        ExecuteTest(*testCase);
    }

    tests.clear();

    Report();
}

bool TestEnv::ExecuteTest(TestCollection& testCollection)
{
#ifdef __DEBUG__
    testCollection.Start();
#else // _DEBUG__
    try
    {
        testCollection.Start();
    }
    catch (Exception)
    {
        auto log = Logger::Get("TestEnv");
        log.OutError([](auto& ls)
        {
            ls << "Exception occurred!";
        });
    }    
#endif // __DEBUG__

    if (!testCollection.IsDone())
    {
        invalidTests.push_back(testCollection.GetName());
    }
    else
    {
        using namespace std;
        std::stringstream ss;

        ++testedCount;

        if (testCollection.IsSuccess())
        {
            ++passCount;

            auto& warnMessages = testCollection.GetWarningMessages();
            for (auto& msg : warnMessages)
            {
                ss << '[' << testCollection.GetName() << "] " << msg;
                warningMessages.push_back(ss.str());
                ss.str("");
            }
        }
        else
        {
            ss << testCollection.GetName() << " : [FAIL]";
            failedTests.push_back(ss.str());
            ss.str("");

            auto& warnMessages = testCollection.GetWarningMessages();
            for (auto& msg : warnMessages)
            {
                ss << '[' << testCollection.GetName() << "]" << msg;
                warningMessages.push_back(ss.str());
                ss.str("");
            }
            
            auto& errMessages = testCollection.GetErrorMessages();
            for (auto& msg : errMessages)
            {
                ss << '[' << testCollection.GetName() << "]" << msg;
                errorMessages.push_back(ss.str());
                ss.str("");
            }
        }
    }

    return testCollection.IsSuccess();
}

void TestEnv::Report()
{
    using namespace std;
    auto log = Logger::Get("TestEnv");
    
    log.Out([this](auto& ls)
    {
        ls << hendl;
        ls << "##### TEST COMPLETED #####" << hendl;
        ls << "# Total Count = " << tests.size() << hendl;
        ls << "# Test Done = " << testedCount << hendl;
        ls << "# Invalid Test = " << invalidTests.size() << hendl;
        ls << "# Pass = " << passCount << hendl;
        ls << "# Fail = " << failedTests.size() << hendl;
        ls << "##### TEST Report Done #####" << hendl;
    });
    
    if (!invalidTests.empty())
    {
        log.OutError("= Invalid Tests =============================");

        for (auto& item : invalidTests)
        {
            log.OutError([&item](auto& ls) { ls << item; });
        }

        log.OutError("=============================================");
    }
    
    if (!failedTests.empty())
    {
        log.OutError("= Failed Tests ==============================");

        int index = 1;
        for (auto& item : failedTests)
        {
            log.OutError([index, &item](auto& ls)
            {
                ls << index << ": "<< item;
            });

            ++index;
        }

        log.OutError("=============================================\n");
    }

    if (!warningMessages.empty())
    {
        log.OutWarning("= Warnings ==================================");

        for (auto& item : warningMessages)
        {
            log.OutWarning([&item](auto& ls) { ls << item; });
        }
        
        log.OutWarning("=============================================\n");
    }

    if (!errorMessages.empty())
    {
        log.OutError("= Errors ====================================");

        for (auto& item : errorMessages)
        {
            log.OutError([&item](auto& ls) { ls << item; });
        }

        log.OutError("=============================================\n");
    }
}
