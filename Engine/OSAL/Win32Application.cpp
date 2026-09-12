// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Application.h"

#ifdef PLATFORM_WINDOWS

namespace OS
{

Application::Application() noexcept
	: m_platformHandle(nullptr)
{
}

Application::~Application()
{
}

void Application::Initialize()
{
}

void Application::PollEvents()
{
}

} // namespace OS

#endif // PLATFORM_WINDOWS
