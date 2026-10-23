// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <memory>


namespace OS
{
/// API reference: docs/OSAL/Application/index.html
class Application
{
private:
	void* platformHandle;

public:
	Application() noexcept;
	~Application();

	void Initialize();
	void PollEvents();
};

[[nodiscard]] std::unique_ptr<Application> CreateApplication() noexcept;
} // namespace OS
