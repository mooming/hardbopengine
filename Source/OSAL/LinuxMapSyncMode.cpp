// Created by mooming.go@gmail.com 2022

#include "OSMapSyncMode.h"

#include "Config/BuildConfig.h"

#ifdef PLATFORM_LINUX
#include <sys/mman.h>

namespace OS
{

void MapSyncMode::SetAsync()
{
    value |= MS_ASYNC;
}

void MapSyncMode::SetSync()
{
    value |= MS_SYNC;
}

void MapSyncMode::Invalidate()
{
    value |= MS_INVALIDATE;
}

} // namespace OS

#endif // PLATFORM_LINUX
