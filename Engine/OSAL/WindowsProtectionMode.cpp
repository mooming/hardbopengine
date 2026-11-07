// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "OSProtectionMode.h"

#include "Config/BuildConfig.h"
#include "WindowsIntrinsic.h"

#if PLATFORM_WINDOWS

namespace OS
{
void ProtectionMode::SetForbidden() noexcept
{
	value |= ProtectionNone;
}

void ProtectionMode::SetReadable() noexcept
{
	value |= ProtectionRead;
}

void ProtectionMode::SetWritable() noexcept
{
	value |= ProtectionWrite;
}

void ProtectionMode::SetExecutable() noexcept
{
	value |= ProtectionExec;
}
} // namespace OS

#endif // PLATFORM_WINDOWS
