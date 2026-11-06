// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstdint>

#include "ProcessWaitResult.h"


namespace OS
{
using ProcessHandle = void*;
struct ProcessSpawnDesc;
ProcessHandle SpawnProcess(const ProcessSpawnDesc& desc) noexcept;
void KillProcess(ProcessHandle& handle) noexcept;
EProcessWaitResult WaitForProcess(ProcessHandle& handle, uint32_t timeoutMilliSecs, int& exitCode) noexcept;
} // namespace OS

#ifdef TEST_ENABLED
#include "Test/TestCollection.h"

namespace hbe
{
class OSProcessTest final : public TestCollection
{
public:
	OSProcessTest()
		: TestCollection("OSProcessTest")
	{
	}

protected:
	void Prepare() override;
};
} // namespace hbe
#endif // TEST_ENABLED
