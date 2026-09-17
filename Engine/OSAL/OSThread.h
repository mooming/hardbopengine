// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <chrono>
#include <thread>

#include "Config/BuildConfig.h"

namespace OS
{
/// @brief Yield the current thread's time slice to the scheduler.
void Yield() noexcept;
/// @brief Sleep for the specified number of milliseconds.
void Sleep(uint32_t milliseconds) noexcept;

/// @brief Get the current CPU core index.
[[nodiscard]] int GetCPUIndex() noexcept;
/// @brief Get the priority of a thread.
[[nodiscard]] int GetThreadPriority(std::thread& thread) noexcept;
/// @brief Set CPU affinity mask for a thread.
void SetThreadAffinity(std::thread& thread, uint64_t mask) noexcept;
/// @brief Set priority for a thread.
void SetThreadPriority(std::thread& thread, int priority) noexcept;

/// @brief CPU time the calling thread has consumed so far, user plus system time combined.
/// @details Busy time, not wall time: a thread that sleeps or waits on a lock advances this by almost
///          nothing. Task budgets are charged from it precisely so that work which is merely waiting
///          cannot spend a budget it never executed.
/// @note Meaningful only on the thread that did the work. It is the calling thread that is measured,
///       which is what lets a stream budget itself without a handle on any other thread.
/// @note Windows reports this at the granularity of the system clock tick, so budgets charged from it
///       are coarser there than on POSIX platforms, where the reading is nanosecond-valued.
/// @return Zero if the platform call failed. A budget then charges nothing and fails open, which is
///         preferred over a failure mode that stops a stream from taking work at all.
[[nodiscard]] std::chrono::nanoseconds GetThreadCPUTime() noexcept;
} // namespace OS

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{

/// @brief Test collection for OS thread operations.
class OSThreadTest final : public TestCollection
{
public:
	OSThreadTest()
		: TestCollection("OSThreadTest")
	{
	}

protected:
	void Prepare() override;
};

} // namespace hbe
#endif //__UNIT_TEST__
