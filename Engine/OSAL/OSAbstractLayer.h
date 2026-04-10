// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

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

	hbe::HString GetFullPath(const hbe::HString& path);

	bool IsDirectory(const char* path);

	hbe::HVector<hbe::HString> ListFilesInDirectory(const char* path);

} // namespace OS
