// Created by mooming.go@gmail.com 2022

#include "OSAbstractLayer.h"

#ifdef PLATFORM_WINDOWS
#include "String/StringUtil.h"
#include <cstdio>
#include <cstdlib>

using namespace std;
using namespace HSTL;

namespace OS
{

    HString GetFullPath(const HString &path)
    {
        using namespace StringUtil;

        char fullPath[MAX_PATH];
        GetFullPathNameA(path.c_str(), MAX_PATH, fullPath, NULL);

        return TrimPath(fullPath);
    }

    bool IsDirectory(const char *path)
    {
        DWORD fileType = GetFileAttributesA(path);
        if (fileType == INVALID_FILE_ATTRIBUTES)
        {
            return false;
        }

        if ((fileType & FILE_ATTRIBUTE_DIRECTORY) == 0)
        {
            return false;
        }

        return true;
    }

    HVector<HString> ListFilesInDirectory(const char *path)
    {
        HVector<HString> fileList;

        WIN32_FIND_DATAA fileData;
        memset(&fileData, 0, sizeof(WIN32_FIND_DATAA));

        auto filesPath = StringUtil::TrimPath(path);
        filesPath.append("/*");

        auto handle = FindFirstFileA(filesPath.c_str(), &fileData);
        bool findSelf = false;
        bool findParent = false;
        while (handle != INVALID_HANDLE_VALUE)
        {
            if (findSelf || strcmp(fileData.cFileName, ".") != 0)
            {
                if (findParent || strcmp(fileData.cFileName, "..") != 0)
                {
                    HString file(fileData.cFileName);
                    fileList.push_back(file);
                }
                else
                {
                    findParent = true;
                }
            }
            else
            {
                findSelf = true;
            }

            if (FindNextFileA(handle, &fileData) == false)
            {
                break;
            }
        }

        FindClose(handle);

        fileList.shrink_to_fit();

        return fileList;
    }

} // namespace OS

#endif // PLATFORM_WINDOWS
