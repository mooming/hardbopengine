// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "OSFileOpenMode.h"

#include "Config/BuildConfig.h"

#ifdef PLATFORM_LINUX
#include <fcntl.h>

void OS::FileOpenMode::setReadOnly() noexcept { value |= O_RDONLY; }

void OS::FileOpenMode::setWriteOnly() noexcept { value |= O_WRONLY; }

void OS::FileOpenMode::setReadWrite() noexcept { value |= O_RDWR; }

void OS::FileOpenMode::setCreate() noexcept { value |= O_CREAT; }

void OS::FileOpenMode::setTruncate() noexcept { value |= O_TRUNC; }

void OS::FileOpenMode::setAppend() noexcept { value |= O_APPEND; }
#endif // PLATFORM_LINUX
