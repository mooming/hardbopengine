// Created by mooming.go@gmail.com 2022

#include "OSThread.h"

#include "Engine.h"
#include <thread>


void OS::Yield()
{
    std::this_thread::yield();
}

void OS::Sleep(uint32_t milliseconds)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

#ifdef __UNIT_TEST__

void HE::OSThreadTest::Prepare()
{
    // TODO
}

#endif //__UNIT_TEST__
