// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "Config/BuildConfig.h"

#ifdef PLATFORM_WINDOWS
#include "Window.h"

namespace OS
{

class Win32Window final : public IWindow
{
public:
	Win32Window();
	~Win32Window() override;

	bool CreateWindow(const hbe::HString& title, int width, int height) override;
	void SetTitle(const hbe::HString& title) override;
	void SetSize(int width, int height) override;

	[[nodiscard]] int GetWidth() const override;
	[[nodiscard]] int GetHeight() const override;
	[[nodiscard]] bool IsVisible() const override;

	void SetVisible(bool visible) override;
	void PollEvents() override;
	void Close() override;

	[[nodiscard]] bool IsClosed() const override;
	[[nodiscard]] intptr_t GetNativeHandle() const override;

private:
	static long WindowProc(void* hwnd, unsigned int uMsg, unsigned long long wParam, long lParam);

	void* hwnd;
	int width;
	int height;
	bool visibleFlag;
	bool closedFlag;
	bool shouldCloseFlag;
};

} // namespace OS
#endif // PLATFORM_WINDOWS
