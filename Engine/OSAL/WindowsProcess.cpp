// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Config/BuildConfig.h"
#include "OSFileHandle.h"
#include "OSProcess.h"

#if PLATFORM_WINDOWS

namespace OS
{
namespace
{
FileHandle stubFileHandle;
} // namespace

Process::Process()
{
}

void Process::Kill() noexcept
{
}

bool Process::Spawn(const ProcessSpawnDesc&)
{
	return false;
}

const FileHandle& Process::GetStdIn() const noexcept
{
	return stubFileHandle;
}

const FileHandle& Process::GetStdOut() const noexcept
{
	return stubFileHandle;
}

const FileHandle& Process::GetStdErr() const noexcept
{
	return stubFileHandle;
}

EProcessWaitResult Process::Wait(uint32_t, int&) noexcept
{
	return EProcessWaitResult::Failed;
}
} // namespace OS

#endif // PLATFORM_WINDOWS
