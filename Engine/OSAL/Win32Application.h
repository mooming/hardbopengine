// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.


#pragma once

#ifdef PLATFORM_WINDOWS

#include "Application.h"

namespace OS
{

class Win32Application final : public IApplication
{
public:
	Win32Application();
	~Win32Application() override;

	void Initialize() override;
	void PollEvents() override;
};

} // namespace OS

#endif // PLATFORM_WINDOWS
