// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "OSThread.h"

#include <chrono>
#include <thread>

#include "Engine/Engine.h"
#include "OSMemory.h"


void OS::Yield() noexcept
{
	std::this_thread::yield();
}

void OS::Sleep(uint32_t milliseconds) noexcept
{
	std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

#ifdef __UNIT_TEST__

void hbe::OSThreadTest::Prepare()
{
	AddTest("Yield", [this](auto& ls)
	{
		OS::Yield();
		ls << "Yield succeeded" << lf;
	});

	AddTest("Sleep", [this](auto& ls)
	{
		auto start = std::chrono::high_resolution_clock::now();
		OS::Sleep(10);
		auto end = std::chrono::high_resolution_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

		ls << "Sleep(10ms) elapsed: " << elapsed << "ms" << lf;

		if (elapsed < 5)
		{
			ls << "Sleep completed too quickly" << lferr;
		}
	});

	AddTest("Thread CPU Time Advances When Busy", [this](auto& ls)
	{
		const auto start = OS::GetThreadCPUTime();
		const auto wallStart = std::chrono::steady_clock::now();
		unsigned long long sink = 0;

		while (sink < 400000000ULL && std::chrono::steady_clock::now() - wallStart < std::chrono::milliseconds(20))
		{
			++sink;
		}

		const auto charged = OS::GetThreadCPUTime() - start;
		ls << "20 ms of busy work consumed " << std::chrono::duration_cast<std::chrono::microseconds>(charged).count()
		   << " us of CPU time" << lf;

		if (charged <= std::chrono::nanoseconds::zero())
		{
			ls << "Thread CPU time did not advance across busy work" << lferr;
		}
	});

	AddTest("Thread CPU Time Ignores Sleeping", [this](auto& ls)
	{
		const auto start = OS::GetThreadCPUTime();
		std::this_thread::sleep_for(std::chrono::milliseconds(80));
		const auto charged = OS::GetThreadCPUTime() - start;

		ls << "80 ms of sleep consumed " << std::chrono::duration_cast<std::chrono::microseconds>(charged).count()
		   << " us of CPU time" << lf;

		if (charged > std::chrono::milliseconds(20))
		{
			ls << "Sleeping advanced thread CPU time by "
			   << std::chrono::duration_cast<std::chrono::milliseconds>(charged).count()
			   << " ms - the counter is reporting wall time, which would bill a stream for blocked work" << lferr;
		}
	});

	AddTest("GetCPUIndex", [this](auto& ls)
	{
		auto cpuIndex = OS::GetCPUIndex();
		ls << "CPU Index: " << cpuIndex << lf;

		if (cpuIndex < 0)
		{
			ls << "Invalid CPU index" << lferr;

			return;
		}

		ls << "GetCPUIndex passed" << lf;
	});

	AddTest("GetPageSize", [this](auto& ls)
	{
		auto pageSize = OS::GetPageSize();
		ls << "Page Size: " << pageSize << lf;

		if (pageSize == 0)
		{
			ls << "GetPageSize returned 0" << lferr;

			return;
		}

		if ((pageSize & (pageSize - 1)) != 0)
		{
			ls << "Page size is not a power of 2" << lfwarn;
		}

		ls << "GetPageSize passed" << lf;
	});

	AddTest("Thread Priority Get/Set", [this](auto& ls)
	{
		std::thread testThread([]() { OS::Sleep(5); });

		auto originalPriority = OS::GetThreadPriority(testThread);
		ls << "Original thread priority: " << originalPriority << lf;

		OS::SetThreadPriority(testThread, 5);
		ls << "SetThreadPriority(5) called" << lf;

		auto newPriority = OS::GetThreadPriority(testThread);
		ls << "New thread priority: " << newPriority << lf;

		testThread.join();

		if (newPriority != originalPriority && newPriority != 5)
		{
			ls << "Thread priority not changed as expected" << lferr;

			return;
		}

		ls << "Thread priority test passed" << lf;
	});

	AddTest("Thread Affinity", [this](auto& ls)
	{
		std::thread testThread([]() { OS::Sleep(5); });

		OS::SetThreadAffinity(testThread, 1);
		ls << "Thread affinity set to CPU 1" << lf;

		auto cpuIndex = OS::GetCPUIndex();
		ls << "Current CPU: " << cpuIndex << lf;

		testThread.join();

		OS::SetThreadAffinity(testThread, 0xF);
		ls << "Thread affinity updated to CPU 0-3" << lf;
	});

	AddTest("Multiple Sleep Cycles", [this](auto& ls)
	{
		constexpr int cycles = 5;
		bool allOk = true;

		for (int i = 0; i < cycles; ++i)
		{
			auto start = std::chrono::high_resolution_clock::now();
			OS::Sleep(1);
			auto end = std::chrono::high_resolution_clock::now();
			auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

			if (elapsed < 500)
			{
				ls << "Sleep cycle " << i << " too short: " << elapsed << "us" << lferr;
				allOk = false;
			}
		}

		if (allOk)
		{
			ls << cycles << " sleep cycles completed" << lf;
		}
		else
		{
			ls << "Some sleep cycles failed" << lferr;
		}
	});
}

#endif //__UNIT_TEST__
