// Created by mooming.go@gmail.com, 2017 - 2022

#pragma once

#include <memory>
#include <string>
#include <vector>


namespace HE
{

class TestCollection;

class TestEnv
{
    using TCPtr = std::unique_ptr<TestCollection>;

private:
    std::vector<TCPtr> tests;
    std::vector<std::string> invalidTests;
    std::vector<std::string> failedTests;
    std::vector<std::string> warningMessages;
    std::vector<std::string> errorMessages;
    
    unsigned int testedCount;
    unsigned int passCount;
    
public:
    static TestEnv& GetEnv();
    void Start();

    template <typename T, typename ... Types>
    void AddTestCollection(Types&& ... args)
    {
        tests.push_back(std::make_unique<T>(std::forward(args) ...));
    }

private:
    inline TestEnv()
        : testedCount(0), passCount(0)
    {
    }

    bool ExecuteTest(TestCollection& testCollection);
    void Report();
};
}
