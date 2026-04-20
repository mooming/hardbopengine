// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.


#include "Application.h"

#include "Config/BuildConfig.h"

#ifdef PLATFORM_WINDOWS
#include "Win32Application.h"
#elif defined(PLATFORM_LINUX)
#include "LinuxApplication.h"
#elif defined(PLATFORM_OSX)
#include "OSXApplication.h"
#endif

namespace OS
{

std::unique_ptr<IApplication> CreateApplication()
{
#ifdef PLATFORM_WINDOWS
	return std::make_unique<Win32Application>();
#elif defined(PLATFORM_LINUX)
	return std::make_unique<LinuxApplication>();
#elif defined(PLATFORM_OSX)
	return std::make_unique<OSXApplication>();
#else
	return nullptr;
#endif
}

} // namespace OS
