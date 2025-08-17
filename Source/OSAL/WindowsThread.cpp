// Created by mooming.go@gmail.com 2022

#include "OSThread.h"

#ifdef PLATFORM_WINDOWS
#include <Windows.h>

int OS::GetCPUIndex()
{
    auto index = GetCurrentProcessorNumber();
    return static_cast<int>(index);
}

void OS::SetThreadAffinity(std::thread& thread, uint64_t mask)
{
    ::SetThreadAffinityMask(thread.native_handle(), mask);
}

void OS::SetThreadPriority(std::thread& thread, int priority)
{
    auto result = ::SetThreadPriority(thread.native_handle(), priority);
    if (result == false)
    {
        auto& engine = hbe::Engine::Get();
        engine.LogError([](auto& ls) { ls << "SetThreadPriority failed."; });
    }
}

#endif // PLATFORM_WINDOWS
