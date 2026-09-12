// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "OSFileOpenMode.h"

#include "Config/BuildConfig.h"

#ifdef PLATFORM_OSX
#include <fcntl.h>

namespace OS
{

void FileOpenMode::SetReadOnly() noexcept { value |= O_RDONLY; }

void FileOpenMode::SetWriteOnly() noexcept { value |= O_WRONLY; }

void FileOpenMode::SetReadWrite() noexcept { value |= O_RDWR; }

void FileOpenMode::SetCreate() noexcept { value |= O_CREAT; }

void FileOpenMode::SetTruncate() noexcept { value |= O_TRUNC; }

void FileOpenMode::SetAppend() noexcept { value |= O_APPEND; }

} // namespace OS

#endif // PLATFORM_OSX
