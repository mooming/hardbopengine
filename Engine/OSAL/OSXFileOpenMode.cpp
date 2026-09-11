// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "OSFileOpenMode.h"

#include "Config/BuildConfig.h"

#ifdef PLATFORM_OSX
#include <fcntl.h>

namespace OS
{

void FileOpenMode::setReadOnly() noexcept { value |= O_RDONLY; }

void FileOpenMode::setWriteOnly() noexcept { value |= O_WRONLY; }

void FileOpenMode::setReadWrite() noexcept { value |= O_RDWR; }

void FileOpenMode::setCreate() noexcept { value |= O_CREAT; }

void FileOpenMode::setTruncate() noexcept { value |= O_TRUNC; }

void FileOpenMode::setAppend() noexcept { value |= O_APPEND; }

} // namespace OS

#endif // PLATFORM_OSX
