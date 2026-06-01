// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "Config/BuildConfig.h"

#ifdef PLATFORM_LINUX
#include "Window.h"
#if __has_include(<X11/Xlib.h>)
#include <X11/Xlib.h>
#else
// Minimal stub definitions for X11 types when headers are unavailable
using Display = void*;
using Window = unsigned long;
#endif

namespace OS
{

class LinuxWindow final : public IWindow
{
public:
	LinuxWindow();
	~LinuxWindow() override;

	bool CreateWindow(const hbe::HString& title, int width, int height) override;
	void SetTitle(const hbe::HString& title) override;
	void SetSize(int width, int height) override;

	[[nodiscard]] int GetWidth() const override;
	[[nodiscard]] int GetHeight() const override;
	[[nodiscard]] bool IsVisible() const override;

	void SetVisible(bool visible) override
	{
		if (display && window)
		{
			if (visible)
			{
				XMapWindow(display, window);
			}
			else
			{
				XUnmapWindow(display, window);
			}
		}

		visibleFlag = visible;
	}

	void PollEvents() override;
	void Close() override;

	[[nodiscard]] bool IsClosed() const override;
	[[nodiscard]] intptr_t GetNativeHandle() const override { return static_cast<intptr_t>(window); }

private:
	Display* display;
	Window window;
	int width;
	int height;
	bool visibleFlag;
	bool closedFlag;
};

} // namespace OS
#endif // PLATFORM_LINUX
