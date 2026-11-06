// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Config/BuildConfig.h"
#include "OSProcessHandle.h"

#if PLATFORM_LINUX
namespace OS
{
ProcessHandle::ProcessHandle() noexcept : pid(0)
{
}

ProcessHandle::ProcessHandle(ProcessHandle&& rhs) noexcept
{
	pid = rhs.pid;
	rhs.Invalidate();
}

[[nodiscard]] bool ProcessHandle::IsValid() const noexcept
{
	return pid > 0;
}

void ProcessHandle::Invalidate() noexcept
{
	pid = 0;
}
} // namespace OS
#endif // PLATFORM_LINUX
