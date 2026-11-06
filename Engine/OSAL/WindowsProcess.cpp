// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Config/BuildConfig.h"
#include "ProcessHandle.h"

#ifdef PLATFORM_WINDOWS

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
	return data != nullptr;
}

void ProcessHandle::Invalidate() noexcept
{
	data = nullptr;
}
} // namespace OS
#endif // PLATFORM_WINDOWS
