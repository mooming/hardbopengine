// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Log/Logger.h"
#include "OSThread.h"

#if PLATFORM_WINDOWS
#include <Windows.h>

int OS::GetCPUIndex() noexcept
{
	auto index = GetCurrentProcessorNumber();

	return static_cast<int>(index);
}

int OS::GetThreadPriority(std::thread& thread) noexcept
{
	return ::GetThreadPriority(thread.native_handle());
}

void OS::SetThreadAffinity(std::thread& thread, uint64_t mask) noexcept
{
	::SetThreadAffinityMask(thread.native_handle(), mask);
}

void OS::SetThreadPriority(std::thread& thread, int priority) noexcept
{
	auto result = ::SetThreadPriority(thread.native_handle(), priority);
	if (result == false)
	{
		const auto log = hbe::Logger::Get("OS::Thread");
		log.OutError([](auto& ls) { ls << "SetThreadPriority failed."; });
	}
}

namespace
{
	double GetCycleRateHertz() noexcept
	{
		static const double rate = []
		{
			ULONGLONG startCycles = 0;
			ULONGLONG endCycles = 0;
			LARGE_INTEGER startCounter{};
			LARGE_INTEGER endCounter{};
			LARGE_INTEGER frequency{};

			QueryPerformanceFrequency(&frequency);
			QueryThreadCycleTime(GetCurrentThread(), &startCycles);
			QueryPerformanceCounter(&startCounter);

			volatile uint64_t sink = 0;
			for (uint64_t counter = 0; counter < 40000000ULL; ++counter)
			{
				sink += counter % 7U;
			}

			QueryPerformanceCounter(&endCounter);
			QueryThreadCycleTime(GetCurrentThread(), &endCycles);

			const double seconds = static_cast<double>(endCounter.QuadPart - startCounter.QuadPart) /
				static_cast<double>(frequency.QuadPart);

			return seconds > 0.0 ? static_cast<double>(endCycles - startCycles) / seconds : 1.0;
		}();

		return rate;
	}
} // namespace

std::chrono::nanoseconds OS::GetThreadCPUTime() noexcept
{
	ULONGLONG cycles = 0;
	if (QueryThreadCycleTime(GetCurrentThread(), &cycles) == 0)
	{
		return std::chrono::nanoseconds::zero();
	}

	const std::chrono::duration<double> seconds{cycles / GetCycleRateHertz()};

	return std::chrono::duration_cast<std::chrono::nanoseconds>(seconds);
}

#endif // PLATFORM_WINDOWS
