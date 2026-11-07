// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <memory>

#include "Config/BuildConfig.h"
#include "HSTL/HString.h"

#if PLATFORM_WINDOWS
struct HWND__;
#endif


namespace OS
{
/// API reference: docs/OSAL/Window/index.html
class Window
{
private:
	int width;
	int height;
	bool visibleFlag;
	bool closedFlag;

#if PLATFORM_LINUX
	void* display;
	intptr_t window;
#elif PLATFORM_WINDOWS
	void* hwnd;
	bool shouldCloseFlag;

	static long long __stdcall WindowProc(HWND__* hwnd, unsigned int uMsg, unsigned long long wParam, long long lParam);
#elif PLATFORM_OSX
	void* osHandle;
	void* osDelegate;
#endif

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
};

[[nodiscard]] std::unique_ptr<Window> CreateWindow(const hbe::HString& title, int width, int height);
} // namespace OS

#ifdef TEST_ENABLED
#include "Test/TestCollection.h"

namespace hbe
{
class WindowTest final : public TestCollection
{
public:
	WindowTest()
		: TestCollection("WindowTest")
	{
	}

protected:
	void Prepare() override;
};
} // namespace hbe
#endif // TEST_ENABLED
