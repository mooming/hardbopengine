// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.


#pragma once

#ifdef PLATFORM_LINUX

#include "Application.h"

namespace OS
{

class LinuxApplication final : public IApplication
{
public:
	LinuxApplication();
	~LinuxApplication() override;

	void Initialize() override;
	void PollEvents() override;
};

} // namespace OS

#endif // PLATFORM_LINUX
