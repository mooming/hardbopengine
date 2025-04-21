// Created by mooming.go@gmail.com 2022

#pragma once

#include "Config/BuildConfig.h"
#include <thread>

namespace OS
{
void Yield();
void Sleep(uint32_t milliseconds);

int GetCPUIndex();
int GetThreadPriority(std::thread& thread);
void SetThreadAffinity(std::thread& thread, uint64_t mask);
void SetThreadPriority(std::thread& thread, int priority);
} // namespace OS

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace HE
{

class OSThreadTest : public TestCollection
{
public:
    OSThreadTest()
        : TestCollection("OSThreadTest")
    {
    }

protected:
    virtual void Prepare() override;
};

} // namespace HE
#endif //__UNIT_TEST__
