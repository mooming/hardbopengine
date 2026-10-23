// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <chrono>
#include <thread>

#include "Config/BuildConfig.h"


namespace OS
{
/// API reference: docs/OSAL/OSThread/index.html
void Yield() noexcept;
void Sleep(uint32_t milliseconds) noexcept;

[[nodiscard]] int GetCPUIndex() noexcept;
[[nodiscard]] int GetThreadPriority(std::thread& thread) noexcept;
void SetThreadAffinity(std::thread& thread, uint64_t mask) noexcept;
void SetThreadPriority(std::thread& thread, int priority) noexcept;

[[nodiscard]] std::chrono::nanoseconds GetThreadCPUTime() noexcept;
} // namespace OS

#ifdef TEST_ENABLED
#include "Test/TestCollection.h"

namespace hbe
{
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
#endif // TEST_ENABLED
