// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "Config/BuildConfig.h"

#ifdef PLATFORM_OSX
#include "Window.h"

namespace OS
{

class OSXWindow final : public IWindow
{
public:
	OSXWindow();
	~OSXWindow() override;

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

	void SetPixels(const uint32_t* pixels, int width, int height) override;

private:
	void* nsWindow;
	int width;
	int height;
	bool visibleFlag;
	bool closedFlag;
};

} // namespace OS
#endif // PLATFORM_OSX
