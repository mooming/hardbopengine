// Created by mooming.go@gmail.com

#pragma once

#include <string>
#include <vector>
#include "File.h"

namespace OS
{
	class Directory
	{
		using Dirs = std::vector<Directory>;

	public:
		const std::string path;

	private:
		Files fileList;
		Dirs dirList;

	public:
		Directory(const char* path);
		virtual ~Directory() = default;

		inline const Files& FileList() const { return fileList; }
		inline const Dirs& DirList() const { return dirList; }
	};

	using Dirs = std::vector<Directory>;
} // namespace OS
