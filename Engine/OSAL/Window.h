// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <memory>

#include "Config/BuildConfig.h"
#include "HSTL/HString.h"

namespace OS
{

/// @brief Platform-independent window facade.
/// @details Concrete behaviour differs per OS and is implemented in the per-platform
///          units (LinuxWindow.cpp, OSXWindow.mm, Win32Window.cpp), selected at build
///          time via `#if defined(PLATFORM_*)`. The public interface is identical.
class Window
{
public:
	Window() noexcept;
	~Window();

	[[nodiscard]] bool CreateWindow(const hbe::HString& title, int width, int height);
	void SetTitle(const hbe::HString& title);
	void SetSize(int width, int height);
	void SetVisible(bool visible);
	void PollEvents();
	void Close();

	[[nodiscard]] int GetWidth() const;
	[[nodiscard]] int GetHeight() const;
	[[nodiscard]] bool IsVisible() const;
	[[nodiscard]] bool IsClosed() const;
	[[nodiscard]] intptr_t GetNativeHandle() const;

private:
	int width;
	int height;
	bool visibleFlag;
	bool closedFlag;

#if defined(PLATFORM_LINUX)
	void* display;
	intptr_t window;
#elif defined(PLATFORM_WINDOWS)
	void* hwnd;
	bool shouldCloseFlag;
#elif defined(PLATFORM_OSX)
	void* osHandle;
#endif
};

/// @brief Creates a Window owned by the caller.
[[nodiscard]] std::unique_ptr<Window> CreateWindow(const hbe::HString& title, int width, int height);

} // namespace OS

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{

class WindowTest final : public TestCollection
{
public:
	WindowTest() : TestCollection("WindowTest") {}

protected:
	void Prepare() override;
};

} // namespace hbe
#endif // __UNIT_TEST__
