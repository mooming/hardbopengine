// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Config/BuildConfig.h"
#include "OSProcessHandle.h"

#if defined(PLATFORM_LINUX) || defined(PLATFORM_OSX)

namespace OS
{
ProcessHandle::ProcessHandle() noexcept
{
	Invalidate();
}

ProcessHandle::ProcessHandle(ProcessHandle&& rhs) noexcept
	: data(rhs.data)
{
	rhs.Invalidate();
}

bool ProcessHandle::IsValid() const noexcept
{
	return pid > 0;
}

void ProcessHandle::Invalidate() noexcept
{
	pid = 0;
}
} // namespace OS
#endif
