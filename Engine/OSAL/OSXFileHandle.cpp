// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "OSFileHandle.h"

#include "Config/BuildConfig.h"
#include "Core/CommonMacros.h"
#include "Intrinsic.h"
#include "OSInputOutput.h"

#ifdef PLATFORM_OSX
#include <sys/stat.h>

namespace OS
{

namespace
{

int getHandle(const FileHandle& handle)
{
	auto& fd = reinterpret_cast<const int&>(handle.data);
	return fd;
}

void setHandle(FileHandle& outHandle, int fd)
{
	int& data = reinterpret_cast<int&>(outHandle.data);
	data = fd;
}

} // namespace

FileHandle::FileHandle() { invalidate(); }

FileHandle::FileHandle(FileHandle&& rhs) : data(rhs.data) { rhs.invalidate(); }

FileHandle::~FileHandle()
{
	returnIf(getHandle(*this) < 0);

	Close(std::move(*this));
}

size_t FileHandle::getFileSize() const noexcept
{
	auto fd = getHandle(*this);
	if (unlikely(fd < 0))
		return 0;

	struct stat statValue;
	int result = fstat(fd, &statValue);

	if (unlikely(result != 0))
		return 0;

	static_assert(sizeof(size_t) == sizeof(statValue.st_size));

	return statValue.st_size;
}

bool FileHandle::IsValid() const noexcept
{
	auto fd = getHandle(*this);
	return fd >= 0;
}

void FileHandle::invalidate() noexcept { setHandle(*this, -1); }

} // namespace OS
#endif // PLATFORM_OSX
