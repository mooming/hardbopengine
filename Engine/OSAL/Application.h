// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <memory>

namespace OS
{

/// @brief Platform-independent application facade.
/// @details Public interface is identical on every OS; the behaviour differs per
///          platform and lives in the per-platform implementation units
///          (LinuxApplication.cpp, OSXApplication.mm, Win32Application.cpp),
///          selected at build time via `#if defined(PLATFORM_*)`.
class Application
{
public:
	Application() noexcept;
	~Application();

	void Initialize();
	void PollEvents();

private:
	// Opaque per-platform handle (e.g. NSApplication*, HINSTANCE, ...).
	void* m_platformHandle;
};

/// @brief Creates an Application owned by the caller.
[[nodiscard]] std::unique_ptr<Application> CreateApplication() noexcept;

} // namespace OS
