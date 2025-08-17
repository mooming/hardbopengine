// Created by mooming.go@gmail.com 2022

#include "OSThread.h"

#include "Log/Logger.h"

#ifdef PLATFORM_LINUX
#include <sched.h>
#include <sys/resource.h>


int OS::GetCPUIndex()
{
    return sched_getcpu();
}

void OS::SetThreadAffinity(std::thread& thread, uint64_t mask)
{
    cpu_set_t set;
    CPU_ZERO(&set);
    for (size_t i = 0; i < sizeof(uint64_t); ++i)
    {
        if (((mask >> i) & 1) == 0)
            continue;

        CPU_SET(i, &set);
    }

    if (sched_setaffinity(thread.native_handle(), sizeof(set), &set) != 0)
    {
        const auto log = hbe::Logger::Get("OS::Thread");
        log.OutError([](auto& ls) { ls << "failed to set cpu affinity"; });
    }
}

void OS::SetThreadPriority(std::thread& thread, int priority)
{
    if (setpriority(PRIO_PROCESS, thread.native_handle(), priority) != 0)
    {
        const auto log = hbe::Logger::Get("OS::Thread");
        log.OutError([](auto& ls) { ls << "failed to set thread affinity"; });
    }
}

#endif // PLATFORM_LINUX
