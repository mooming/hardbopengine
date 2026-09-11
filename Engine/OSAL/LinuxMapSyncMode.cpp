// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "OSMapSyncMode.h"

#include "Config/BuildConfig.h"

#ifdef PLATFORM_LINUX
#include <sys/mman.h>

namespace OS
{

void MapSyncMode::setAsync() noexcept { value |= MS_ASYNC; }

void MapSyncMode::setSync() noexcept { value |= MS_SYNC; }

void MapSyncMode::invalidate() noexcept { value |= MS_INVALIDATE; }

} // namespace OS

#endif // PLATFORM_LINUX
