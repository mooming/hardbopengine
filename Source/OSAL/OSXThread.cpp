// Created by mooming.go@gmail.com 2022

#include "OSThread.h"

#ifdef PLATFORM_OSX

/* See feature_test_macros(7) */
#define _GNU_SOURCE

#include "Log/Logger.h"
#include "String/StringUtil.h"
#include <pthread.h>


int OS::GetCPUIndex()
{
    size_t cpuNumber = 0;
    auto result = pthread_cpu_number_np(&cpuNumber);
    if (unlikely(result != 0))
    {
        using namespace HE;
        using namespace StringUtil;
        auto funcName = ToFunctionName(__PRETTY_FUNCTION__);
        auto log = Logger::Get(funcName);

        log.OutError([](auto& ls) { ls << "failed to get cpu number"; });

        return -1;
    }

    return static_cast<int>(cpuNumber);
}

int OS::GetThreadPriority(std::thread& thread)
{
    auto nativeHandle = thread.native_handle();

    int policy = 0;
    struct sched_param sp;

    auto result = pthread_getschedparam(nativeHandle, &policy, &sp);
    if (unlikely(result != 0))
    {
        using namespace HE;
        using namespace StringUtil;
        auto funcName = ToFunctionName(__PRETTY_FUNCTION__);
        auto log = Logger::Get(funcName);

        switch (result)
        {
        case ESRCH:
            log.OutError([](auto& ls) { ls << "Non-existent thread."; });
            break;

        default:
            log.OutError([result](auto& ls) { ls << "Unexpected error code = " << result; });
            break;
        }

        return -1;
    }

    return sp.sched_priority;
}

void OS::SetThreadAffinity(std::thread& thread, uint64_t mask)
{
    // Not supproted on Apple Silicon
}

void OS::SetThreadPriority(std::thread& thread, int priority)
{
    auto nativeHandle = thread.native_handle();

    struct sched_param sp;
    memset(&sp, 0, sizeof(decltype(sp)));
    sp.sched_priority = priority;

    constexpr int policy = SCHED_FIFO;
    auto result = pthread_setschedparam(nativeHandle, policy, &sp);
    if (unlikely(result != 0))
    {
        using namespace HE;
        auto log = Logger::Get("OS::Thread");

        switch (result)
        {
        case EINVAL:
            log.OutError([policy](auto& ls)
                         { ls << "Invalid value for policy. Input policy = " << policy; });
            break;

        case ENOTSUP:
            log.OutError([](auto& ls) { ls << "Invalid value for scheduling parameters."; });
            break;

        case ESRCH:
            log.OutError([](auto& ls) { ls << "Non-existent thread."; });
            break;

        default:
            log.OutError([result](auto& ls) { ls << "Unexpected error code = " << result; });
            break;
        }
    }
}

#endif // PLATFORM_OSX
