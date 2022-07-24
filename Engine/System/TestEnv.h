// Created by mooming.go@gmail.com, 2017

#pragma once

#include <list>

namespace HE
{
    class TestCase;

    class TestEnv
    {

    private:
        std::list<TestCase*> tests;

        unsigned int testedCount;
        unsigned int invalidCount;
        unsigned int passCount;
        unsigned int failCount;

    public:
        static TestEnv& GetEnv();
        void Start();

        void AddTest(TestCase* test);

    private:
        inline TestEnv() : tests(), testedCount(0), invalidCount(0), passCount(0), failCount(0) {}
        bool ExecuteTest(TestCase& testCase);
        void Report();
    };
}
