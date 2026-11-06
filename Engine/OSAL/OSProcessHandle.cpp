// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "OSProcessHandle.h"


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
} // namespace OS
