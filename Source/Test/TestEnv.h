// Created by mooming.go@gmail.com, 2017 - 2022

#pragma once

#include <string>
#include <vector>


namespace HE
{

class TestCollection;

class TestEnv
{
    
private:
    std::vector<TestCollection*> tests;
    std::vector<std::string> invalidTests;
    std::vector<std::string> failedTests;
    std::vector<std::string> warningMessages;
    std::vector<std::string> errorMessages;
    
    unsigned int testedCount;
    unsigned int passCount;
    
public:
    static TestEnv& GetEnv();
    void Start();
    
    void AddTestCollection(TestCollection* test);
    
private:
    inline TestEnv()
        : tests(), testedCount(0), passCount(0)
    {
    }
    
    bool ExecuteTest(TestCollection& testCollection);
    void Report();
};
}
