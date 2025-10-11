// Created by mooming.go@gmail.com 2022

#include "OSAbstractLayer.h"

#ifdef PLATFORM_OSX
#include <cstdio>
#include <cstdlib>
#include "String/StringUtil.h"

using namespace std;
using namespace hbe;

namespace OS
{

	HString GetFullPath(const HString& path)
	{
		using namespace StringUtil;

		char fullPath[PATH_MAX];
		void* ptr = realpath(path.c_str(), fullPath);
		if (ptr != fullPath)
		{
			std::cerr << "[OS::GetFullPath] Failed to get the full path of " << path << std::endl;

			return TrimPath(path);
		}

		return TrimPath(fullPath);
	}

	bool IsDirectory(const char* path)
	{
		DIR* dir = opendir(path);

		if (dir != nullptr)
		{
			closedir(dir);
			return true;
		}

		return false;
	}

	HVector<HString> ListFilesInDirectory(const char* path)
	{
		HVector<HString> fileList;

		if (DIR* dir = opendir(path))
		{
			while (struct dirent* element = readdir(dir))
			{
				if (strcmp(element->d_name, ".") == 0)
				{
					continue;
				}

				if (strcmp(element->d_name, "..") == 0)
				{
					continue;
				}

				if (element->d_name[0] == '.' || element->d_name[0] == '\0')
				{
					continue;
				}

				fileList.push_back(HString(element->d_name));
			}

			closedir(dir);
		}

		fileList.shrink_to_fit();

		return fileList;
	}

} // namespace OS

#endif // PLATFORM_OSX
