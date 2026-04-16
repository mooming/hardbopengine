// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.


#include "Config/BuildConfig.h"
#include "IWindow.h"

#ifdef PLATFORM_WINDOWS
#include "Win32Window.h"
#elif defined(PLATFORM_LINUX)
#include "LinuxWindow.h"
#elif defined(PLATFORM_OSX)
#include "OSXWindow.h"
#endif

namespace OS
{
	IWindow* CreateWindow(const hbe::HString& title, int width, int height)
	{
#ifdef PLATFORM_WINDOWS
		auto window = new Win32Window();
#elif defined(PLATFORM_LINUX)
		auto window = new LinuxWindow();
#elif defined(PLATFORM_OSX)
		auto window = new OSXWindow();
#else
		return nullptr;
#endif

		if (window->CreateWindow(title, width, height))
		{
			return window;
		}

		delete window;

		return nullptr;
	}
} // namespace OS
