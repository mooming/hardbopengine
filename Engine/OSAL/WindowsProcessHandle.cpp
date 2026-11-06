// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Config/BuildConfig.h"
#include "OSProcessHandle.h"

#if PLATFORM_WINDOWS
namespace OS
{
ProcessHandle::ProcessHandle() noexcept : data(nullptr)
{
}

ProcessHandle::ProcessHandle(ProcessHandle&& rhs) noexcept
{
	data = rhs.data;
	rhs.Invalidate();
}

[[nodiscard]] bool ProcessHandle::IsValid() const noexcept
{
	return data != nullptr;
}

void ProcessHandle::Invalidate() noexcept
{
	data = nullptr;
}
} // namespace OS
#endif // PLATFORM_WINDOWS
