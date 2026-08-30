// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Application.h"

namespace OS
{

std::unique_ptr<Application> CreateApplication() noexcept
{
	return std::make_unique<Application>();
}

} // namespace OS
