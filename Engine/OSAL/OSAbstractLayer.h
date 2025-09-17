// Created by mooming.go@gmail.com 2016

#pragma once

#include <iostream>
#include "Config/BuildConfig.h"
#include "HSTL/HString.h"
#include "HSTL/HVector.h"
#include "String/StringUtil.h"

#ifdef WIN32
#define WINDOWS
#else // _WINDOWS
#define POSIX
#endif // WIN32

#ifdef WINDOWS
#include <windows.h>
#define PATH_MAX MAX_PATH
#endif // WINDOWS

#ifdef POSIX
#include <dirent.h>
#endif // POSIX

namespace OS
{

	HSTL::HString GetFullPath(const HSTL::HString& path);

	bool IsDirectory(const char* path);

	HSTL::HVector<HSTL::HString> ListFilesInDirectory(const char* path);

} // namespace OS
