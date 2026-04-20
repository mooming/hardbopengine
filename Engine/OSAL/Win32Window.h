// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.


#pragma once


#include "Config/BuildConfig.h"

#ifdef PLATFORM_WINDOWS
#include "Window.h"

namespace OS
{

/**
 * @brief Windows Window class implementation using Win32 API.
 */
class Win32Window : public IWindow
{
public:
	/**
	 * @brief Default constructor for Win32Window.
	 */
	Win32Window();

	/**
	 * @brief Virtual destructor for Win32Window.
	 */
	~Win32Window() override;

	/**
	 * @brief Creates a window on the Windows platform using Win32 API.
	 * @param title The title of the window.
	 * @param width The initial width in pixels.
	 * @param height The initial height in pixels.
	 * @return True if successful, false otherwise.
	 */
	bool CreateWindow(const hbe::HString& title, int width, int height) override;

	/**
	 * @brief Sets the window title on Windows via Win32 API.
	 * @param title The new title for the window.
	 */
	void SetTitle(const hbe::HString& title) override;

	/**
	 * @brief Resizes the Win32 window to specified dimensions.
	 * @param width New width in pixels.
	 * @param height New height in pixels.
	 */
	void SetSize(int width, int height) override;

	/**
	 * @brief Gets the current width of the Win32 window.
	 * @return The width in pixels.
	 */
	[[nodiscard]] int GetWidth() const override;

	/**
	 * @brief Gets the current height of the Win32 window.
	 * @return The height in pixels.
	 */
	[[nodiscard]] int GetHeight() const override;

	/**
	 * @brief Checks if the Win32 window is visible.
	 * @return True if visible, false otherwise.
	 */
	[[nodiscard]] bool IsVisible() const override;

	/**
	 * @brief Sets the visibility of the Win32 window.
	 * @param visible True to show, false to hide.
	 */
	void SetVisible(bool visible) override;

	/**
	 * @brief Polls for Windows messages via PeekMessage/DispatchMessage.
	 */
	void PollEvents() override;

	/**
	 * @brief Closes and destroys the Win32 window resources.
	 */
	void Close() override;

	/**
	 * @brief Checks if the Win32 window has been closed.
	 * @return True if closed, false otherwise.
	 */
	[[nodiscard]] bool IsClosed() const override;

private:
	static long WindowProc(void* hwnd, unsigned int uMsg, unsigned long long wParam, long lParam);

	void* hwnd = nullptr;
	int width = 0;
	int height = 0;
	bool visibleFlag = true;
	bool closedFlag = false;
	bool shouldCloseFlag = false;
};

} // namespace OS
#endif // PLATFORM_WINDOWS
