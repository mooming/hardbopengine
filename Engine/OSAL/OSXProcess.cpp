// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Config/BuildConfig.h"
#include "Core/CommonMacros.h"
#include "Log/Logger.h"
#include "OSFileHandle.h"
#include "OSProcess.h"

#if PLATFORM_OSX
#include <cerrno>
#include <csignal>
#include <cstring>
#include <spawn.h>


namespace OS
{
namespace
{
FileHandle stubFileHandle;
} // namespace

Process::Process()
	: pid(0)
{
}

void Process::Kill() const
{
	ReturnIf(!IsValid());
	int retCode = kill(pid, SIGKILL);

	if (unlikely(retCode != 0))
	{
		const int error = errno;
		const auto log = hbe::Logger::Get("OS::Process");
		log.OutError([retCode, error, pid = pid](auto& ls)
		{
			ls << "Process Kill (pid:" << pid << ") failed. result = " << retCode << ", reason(" << std::strerror(error)
			   << ')';
		});
	}
}

bool Process::IsValid() const
{
	return pid > 0;
}

bool Process::Spawn(const ProcessSpawnDesc&)
{
	if (IsValid())
	{
		const auto log = hbe::Logger::Get("OS::Process");
		log.OutError([pid = pid](auto& ls)
		{
			ls << "Process (pid:" << pid << ") Already exists.";
		});

		return false;
	}

	pid_t spawnedPid = 0;
	static_assert(sizeof(spawnedPid) == sizeof(pid));

	posix_spawn(&spawnedPid, "", nullptr, nullptr, nullptr, nullptr);

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

#endif // PLATFORM_OSX
