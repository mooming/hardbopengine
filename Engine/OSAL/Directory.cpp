// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Directory.h"

#include "OSAbstractLayer.h"
#include "String/StringUtil.h"

using namespace std;

namespace OS
{
	Directory::Directory(const char* path) : path(StringUtil::TrimPath(path))
	{
		auto list = ListFilesInDirectory(path);

		for (const auto& element : list)
		{
			const char* name = element.c_str();

			if (name[0] == '.')
			{
				continue;
			}

			string childPath(path);
			childPath.append("/");
			childPath.append(name);

			if (IsDirectory(childPath.c_str()))
			{
				dirList.push_back(Directory(childPath.c_str()));
			}
			else
			{
				File file(name);
				fileList.push_back(file);
			}
		}
	}
} // namespace OS
