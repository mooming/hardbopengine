// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <string>
#include <vector>

#include "File.h"

namespace OS
{
/// @brief Represents a directory in the file system with access to files and subdirectories.
class Directory
{
	using Dirs = std::vector<Directory>;

public:
	const std::string path;

	Directory(const char* path);
	virtual ~Directory() = default;

	[[nodiscard]] const Files& FileList() const { return fileList; }
	[[nodiscard]] const Dirs& DirList() const { return dirList; }

private:
	Files fileList;
	Dirs dirList;
};

using Dirs = std::vector<Directory>;
} // namespace OS
