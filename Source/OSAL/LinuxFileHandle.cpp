// Created by mooming.go@gmail.com 2022

#include "OSFileHandle.h"

#include "Config/BuildConfig.h"
#include "Intrinsic.h"
#include "OSInputOutput.h"

#ifdef PLATFORM_LINUX
#include <sys/stat.h>

namespace OS
{
	FileHandle::FileHandle() : data(nullptr) { Invalidate(); }

	FileHandle::FileHandle(FileHandle&& rhs) : data(rhs.data) { rhs.Invalidate(); }

	FileHandle::~FileHandle()
	{
		if (fd < 0)
		{
			return;
		}

		Close(std::move(*this));
	}

	size_t FileHandle::GetFileSize() const
	{
		if (unlikely(fd < 0))
		{
			return 0;
		}

		struct stat statValue;
		int result = fstat(fd, &statValue);

		if (unlikely(result != 0))
		{
			return 0;
		}

		static_assert(sizeof(size_t) == sizeof(statValue.st_size));

		return statValue.st_size;
	}

	bool FileHandle::IsValid() const { return fd >= 0; }

	void FileHandle::Invalidate() { fd = -1; }

} // namespace OS
#endif // PLATFORM_LINUX
