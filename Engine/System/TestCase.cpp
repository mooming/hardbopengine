// Created by mooming.go@gmail.com, 2017

#include "System/TestCase.h"

#include <iostream>
#include <exception>

using namespace HE;

TestCase::TestCase(const char * title) : title(title), isDone(false), isSuccess(false)
{
}

void TestCase::Start()
{
    using namespace std;

    try
    {
        cout << title << " : START ====================" << endl;
        isSuccess = DoTest();
        isDone = true;
    }
    catch (exception& e)
    {
        cout << e.what() << endl;
    }

    Report();
}

void TestCase::Report()
{
    using namespace std;

    if (isSuccess)
    {
        cout << title << " : SUCCESS" << endl;
    }
    else
    {
        cerr << title << " : FAILED" << endl;
    }

    cout << endl;
}
