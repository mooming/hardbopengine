// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "Config/BuildConfig.h"

#if PLATFORM_WINDOWS
#include <windows.h>

namespace OS
{
static constexpr int ProtectionNone = 0;
static constexpr int ProtectionRead = 1;
static constexpr int ProtectionWrite = 2;
static constexpr int ProtectionExec = 4;

static constexpr int MapSyncAsync = 1;
static constexpr int MapSyncSync = 2;
static constexpr int MapSyncInvalidate = 4;

[[nodiscard]] constexpr int ToPageProtection(int value) noexcept
{
	if ((value & ProtectionExec) != 0)
	{
		if ((value & ProtectionWrite) != 0)
			return PAGE_EXECUTE_READWRITE;

		if ((value & ProtectionRead) != 0)
			return PAGE_EXECUTE_READ;

		return PAGE_EXECUTE;
	}

	if ((value & ProtectionWrite) != 0)
		return PAGE_READWRITE;

	if ((value & ProtectionRead) != 0)
		return PAGE_READONLY;

	return PAGE_NOACCESS;
}

[[nodiscard]] constexpr int ToFileMapAccess(int value) noexcept
{
	if ((value & ProtectionWrite) != 0)
		return FILE_MAP_WRITE;

	return FILE_MAP_READ;
}
} // namespace OS
#endif // PLATFORM_WINDOWS
