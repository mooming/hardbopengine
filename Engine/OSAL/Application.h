// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.


#pragma once


#include <memory>

namespace OS
{

class IApplication
{
public:
	virtual ~IApplication() = default;

	virtual void Initialize() = 0;
	virtual void PollEvents() = 0;
};

std::unique_ptr<IApplication> CreateApplication();

} // namespace OS
