// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstddef>

namespace OS
{

class FileHandle final
{
public:
	union
	{
		void* data;
		int fd;
	};

	FileHandle();

	FileHandle(const FileHandle&) = delete;
	FileHandle(FileHandle&& rhs);
	~FileHandle();

	[[nodiscard]] size_t GetFileSize() const noexcept;
	[[nodiscard]] bool IsValid() const noexcept;
	void Invalidate() noexcept;
};

} // namespace OS
