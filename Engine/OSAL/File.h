// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <memory>
#include <ostream>
#include <string>
#include <vector>


namespace OS
{
/// API reference: docs/OSAL/File/index.html
class File
{
private:
	std::string path;

public:
	explicit File(const char* path)
		: path(path)
	{
	}

	File(const File& src)
		: path(src.path)
	{
	}

	File(File&& src) noexcept
		: path(std::move(src.path))
	{
	}

	File& operator=(const File& src)
	{
		path = src.path;

		return *this;
	}

	File& operator=(File&& src) noexcept
	{
		path = std::move(src.path);

		return *this;
	}

	bool operator<(const File& rhs) const
	{
		return path < rhs.path;
	}

	friend std::ostream& operator<<(std::ostream& os, const File& file)
	{
		os << file.path;

		return os;
	}

	[[nodiscard]] const auto& GetPath() const
	{
		return path;
	}
};

using Files = std::vector<File>;
} // namespace OS
