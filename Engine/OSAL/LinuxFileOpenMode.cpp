// Created by mooming.go@gmail.com 2022

#include "OSFileOpenMode.h"

#include "Config/BuildConfig.h"

#ifdef PLATFORM_LINUX
#include <fcntl.h>

void OS::FileOpenMode::SetReadOnly() { value |= O_RDONLY; }

void OS::FileOpenMode::SetWriteOnly() { value |= O_WRONLY; }

void OS::FileOpenMode::SetReadWrite() { value |= O_RDWR; }

void OS::FileOpenMode::SetCreate() { value |= O_CREAT; }

void OS::FileOpenMode::SetTruncate() { value |= O_TRUNC; }

void OS::FileOpenMode::SetAppend() { value |= O_APPEND; }
#endif // PLATFORM_LINUX
