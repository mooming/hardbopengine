// Created by mooming.go@gmail.com 2016

#include "OSAbstractLayer.h"

#include "String/StringUtil.h"
#include <cstdio>
#include <cstdlib>


using namespace std;
using namespace HSTL;

namespace OS
{

HString GetFullPath(const HString& path)
{
    using namespace StringUtil;
    
#ifdef WINDOWS
    char fullPath[MAX_PATH];
    GetFullPathNameA(path.c_str(), MAX_PATH, fullPath, NULL);
    
    return TrimPath(fullPath);
#endif // WINDOWS
    
#ifdef POSIX
    char fullPath[PATH_MAX];
    void* ptr = realpath(path.c_str(), fullPath);
    if (ptr != fullPath)
    {
        std::cerr << "[OS::GetFullPath] Failed to get the full path of "
        << path << std::endl;
        
        return TrimPath(path);
    }
    
    return TrimPath(fullPath);
#endif // POSIX
}

bool IsDirectory(const char* path)
{
#ifdef WINDOWS
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
#endif //WINDOWS
    
#ifdef POSIX
    DIR* dir = opendir(path);
    
    if (dir != nullptr)
    {
        closedir(dir);
        return true;
    }
    
    return false;
#endif //POSIX
}

HVector<HString> ListFilesInDirectory(const char* path)
{
    HVector<HString> fileList;
    
#ifdef WINDOWS
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
            break;
    }
    
    FindClose(handle);
    
#endif //WINDOWS
    
#ifdef POSIX
    if (DIR* dir = opendir(path))
    {
        while (struct dirent* element = readdir(dir))
        {
            if (strcmp(element->d_name, ".") == 0)
                continue;
            
            if (strcmp(element->d_name, "..") == 0)
                continue;
            
            if (element->d_name[0] == '.' || element->d_name[0] == '\0')
                continue;
            
            fileList.push_back(HString(element->d_name));
        }
        
        closedir(dir);
    }
#endif //POSIX
    
    fileList.shrink_to_fit();
    
    return fileList;
}

} // OS
