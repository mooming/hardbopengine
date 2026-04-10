// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "OSMapSyncMode.h"

#include "Config/BuildConfig.h"

#ifdef PLATFORM_OSX
#include <sys/mman.h>

namespace OS
{

	void MapSyncMode::SetAsync() { value |= MS_ASYNC; }

	void MapSyncMode::SetSync() { value |= MS_SYNC; }

	void MapSyncMode::Invalidate() { value |= MS_INVALIDATE; }

} // namespace OS

#endif // PLATFORM_OSX
