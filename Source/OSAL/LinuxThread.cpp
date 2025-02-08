// Created by mooming.go@gmail.com 2022

#include "OSThread.h"

#ifdef PLATFORM_LINUX
#include <sched.h>

int OS::GetCPUIndex()
{
    return sched_getcpu();
}

void OS::SetThreadAffinity(std::thread &thread, uint64_t mask)
{
    static_assert(false, "Not implemented yet.");
}

void SetThreadPriority(std::thread &thread, int priority)
{
    static_assert(false, "Not implemented yet.");
}

#endif // PLATFORM_LINUX
