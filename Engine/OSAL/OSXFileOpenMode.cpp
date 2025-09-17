// Created by mooming.go@gmail.com 2022

#include "OSFileOpenMode.h"

#include "Config/BuildConfig.h"

#ifdef PLATFORM_OSX
#include <fcntl.h>

namespace OS
{

	void FileOpenMode::SetReadOnly() { value |= O_RDONLY; }

	void FileOpenMode::SetWriteOnly() { value |= O_WRONLY; }

	void FileOpenMode::SetReadWrite() { value |= O_RDWR; }

	void FileOpenMode::SetCreate() { value |= O_CREAT; }

	void FileOpenMode::SetTruncate() { value |= O_TRUNC; }

	void FileOpenMode::SetAppend() { value |= O_APPEND; }

} // namespace OS

#endif // PLATFORM_OSX
