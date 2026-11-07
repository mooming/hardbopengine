// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "OSMapSyncMode.h"

#include "Config/BuildConfig.h"
#include "WindowsIntrinsic.h"

#if PLATFORM_WINDOWS

namespace OS
{
void MapSyncMode::SetAsync() noexcept
{
	value |= MapSyncAsync;
}

void MapSyncMode::SetSync() noexcept
{
	value |= MapSyncSync;
}

void MapSyncMode::Invalidate() noexcept
{
	value |= MapSyncInvalidate;
}
} // namespace OS

#endif // PLATFORM_WINDOWS
