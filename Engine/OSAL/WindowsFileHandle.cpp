// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "OSFileHandle.h"

#include "Config/BuildConfig.h"
#include "Core/CommonMacros.h"
#include "OSInputOutput.h"

#if PLATFORM_WINDOWS
#include <windows.h>

namespace OS
{
FileHandle::FileHandle()
{
	Invalidate();
}

FileHandle::FileHandle(FileHandle&& rhs)
	: data(rhs.data)
{
	rhs.Invalidate();
}

FileHandle::~FileHandle()
{
	returnIf(data == nullptr);

	Close(std::move(*this));
}

size_t FileHandle::GetFileSize() const noexcept
{
	if (unlikely(data == nullptr))
		return 0;

	LARGE_INTEGER fileSize{};
	if (unlikely(GetFileSizeEx(static_cast<HANDLE>(data), &fileSize) == FALSE))
		return 0;

	return static_cast<size_t>(fileSize.QuadPart);
}

bool FileHandle::IsValid() const noexcept
{
	return data != nullptr;
}

void FileHandle::Invalidate() noexcept
{
	data = nullptr;
}
} // namespace OS

#endif // PLATFORM_WINDOWS
