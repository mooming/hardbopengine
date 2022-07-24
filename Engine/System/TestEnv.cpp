// Created by mooming.go@gmail.com, 2017

#include "System/TestEnv.h"

#include "System/Debug.h"
#include "System/Exception.h"
#include "System/TestCase.h"

#include <iostream>

using namespace HE;

TestEnv& TestEnv::GetEnv()
{
    static TestEnv instance;
    return instance;
}

void TestEnv::Start()
{
    for (auto testCase : tests)
    {
        Assert(testCase != nullptr);
        ExecuteTest(*testCase);
    }

    Report();
}

void HE::TestEnv::AddTest(TestCase* test)
{
    Assert(test != nullptr);
    tests.push_back(test);
}

bool TestEnv::ExecuteTest(TestCase& testCase)
{
#ifdef __DEBUG__
    testCase.Start();
#else // _DEBUG__
    try
    {
        testCase.Start();
    }
    catch (Exception)
    {
        std::cout << "Exception occurred!" << std::endl;
    }    
#endif // __DEBUG__

    if (!testCase.IsDone())
    {
        ++invalidCount;
    }
    else
    {
        ++testedCount;

        if (testCase.IsSuccess())
        {
            ++passCount;
        }
        else
        {
            ++failCount;
        }
    }

    return testCase.IsSuccess();
}

void TestEnv::Report()
{
    using namespace std;

    cout << "##### TEST COMPLETED #####" << endl;
    cout << "# Total Count = " << tests.size() << endl;
    cout << "# Test Done = " << testedCount << endl;
    cout << "# Invalid Test = " << invalidCount << endl;
    cout << "# Pass = " << passCount << endl;
    cout << "# Fail = " << failCount << endl;
    cout << "##### TEST Report Done #####" << endl;
}
