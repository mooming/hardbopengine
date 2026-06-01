// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "OSFileOpenMode.h"
#include "Config/BuildConfig.h"

#include "Config/BuildConfig.h"

#ifdef PLATFORM_LINUX
#include <fcntl.h>

void OS::FileOpenMode::SetReadOnly() noexcept { value |= O_RDONLY; }

void OS::FileOpenMode::SetWriteOnly() noexcept { value |= O_WRONLY; }

void OS::FileOpenMode::SetReadWrite() noexcept { value |= O_RDWR; }

void OS::FileOpenMode::SetCreate() noexcept { value |= O_CREAT; }

void OS::FileOpenMode::SetTruncate() noexcept { value |= O_TRUNC; }

void OS::FileOpenMode::SetAppend() noexcept { value |= O_APPEND; }
#endif // PLATFORM_LINUX
