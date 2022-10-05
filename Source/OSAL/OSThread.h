// Created by mooming.go@gmail.com 2022

#pragma once

#include <thread>


namespace OS
{
int GetCPUIndex();
int GetThreadPriority(std::thread& thread);
void SetThreadAffinity(std::thread& thread, int coreIndex);
void SetThreadPriority(std::thread& thread, int priority);
} // OS

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace HE
{

class OSThreadTest : public TestCollection
{
public:
    OSThreadTest() : TestCollection("OSThreadTest") {}
    
protected:
    virtual void Prepare() override;
};

} // HE
#endif //__UNIT_TEST__
