// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "OSThread.h"

#include "Core/CommonMacros.h"
#include "Log/Logger.h"

#ifdef PLATFORM_LINUX
#include <ctime>
#include <sched.h>
#include <sys/resource.h>

int OS::GetCPUIndex() noexcept
{
	return sched_getcpu();
}

void OS::SetThreadAffinity(std::thread& thread, uint64_t mask) noexcept
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

void OS::SetThreadPriority(std::thread& thread, int priority) noexcept
{
	if (setpriority(PRIO_PROCESS, thread.native_handle(), priority) != 0)
	{
		const auto log = hbe::Logger::Get("OS::Thread");
		log.OutError([](auto& ls) { ls << "failed to set thread affinity"; });
	}
}

std::chrono::nanoseconds OS::GetThreadCPUTime() noexcept
{
	struct timespec timeValue{};
	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &timeValue) != 0)
	{
		return std::chrono::nanoseconds::zero();
	}

	return std::chrono::seconds(timeValue.tv_sec) + std::chrono::nanoseconds(timeValue.tv_nsec);
}

#endif // PLATFORM_LINUX
