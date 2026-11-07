// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Application.h"

#include "Config/BuildConfig.h"

#if PLATFORM_WINDOWS
#include <windows.h>
#include <timeapi.h>

namespace OS
{
Application::Application() noexcept
	: platformHandle(nullptr)
{
}

Application::~Application()
{
	timeEndPeriod(1);
}

void Application::Initialize()
{
	timeBeginPeriod(1);
}

void Application::PollEvents()
{
}
} // namespace OS

#endif // PLATFORM_WINDOWS
