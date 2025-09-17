// Created by mooming.go@gmail.com 2022

#pragma once

#include <cstddef>

namespace OS
{

	struct FileHandle final
	{
	public:
		union
		{
			void* data;
			int fd;
		};

	public:
		FileHandle(const FileHandle&) = delete;

	public:
		FileHandle();
		FileHandle(FileHandle&& rhs);
		~FileHandle();

		size_t GetFileSize() const;
		bool IsValid() const;
		void Invalidate();
	};

} // namespace OS
