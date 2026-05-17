// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

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
} // namespace OS

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{

/// @brief Test collection for OS thread operations.
class OSThreadTest final : public TestCollection
{
public:
	OSThreadTest() : TestCollection("OSThreadTest") {}

protected:
	void Prepare() override;
};

} // namespace hbe
#endif //__UNIT_TEST__
