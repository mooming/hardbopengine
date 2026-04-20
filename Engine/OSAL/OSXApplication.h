// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.


#pragma once

#ifdef PLATFORM_OSX

#include "Application.h"

namespace OS
{

class OSXApplication final : public IApplication
{
private:
	void* appHandle;

public:
	OSXApplication();
	~OSXApplication() override;

	void Initialize() override;
	void PollEvents() override;
};

} // namespace OS

#endif // PLATFORM_OSX
