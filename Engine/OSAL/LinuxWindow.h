// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.


#pragma once


#include "Config/BuildConfig.h"

#ifdef PLATFORM_LINUX
#include "IWindow.h"
#include <X11/Xlib.h>

namespace OS
{

/**
 * @brief Window implementation for Linux using X11.
 */
class LinuxWindow : public IWindow
{
public:
	/**
	 * @brief Default constructor for LinuxWindow.
	 */
	LinuxWindow();

	/**
	 * @brief Virtual destructor for LinuxWindow.
	 */
	~LinuxWindow() override;

	/**
	 * @brief Creates a window on the Linux platform using X11.
	 * @param title The title of the window.
	 * @param width The initial width in pixels.
	 * @param height The initial height in pixels.
	 * @return True if successful, false otherwise.
	 */
	bool CreateWindow(const hbe::HString& title, int width, int height) override;

	/**
	 * @brief Sets the window title on Linux via X11.
	 * @param title The new title for the window.
	 */
	void SetTitle(const hbe::HString& title) override;

	/**
	 * @brief Resizes the Linux window to specified dimensions.
	 * @param width New width in pixels.
	 * @param height New height in pixels.
	 */
	void SetSize(int width, int height) override;

	/**
	 * @brief Gets the current width of the Linux window.
	 * @return The width in pixels.
	 */
	[[nodiscard]] int GetWidth() const override;

	/**
	 * @brief Gets the current height of the Linux window.
	 * @return The height in pixels.
	 */
	[[nodiscard]] int GetHeight() const override;

	/**
	 * @brief Checks if the Linux window is visible.
	 * @return True if visible, false otherwise.
	 */
	[[nodiscard]] bool IsVisible() const override;

	/**
	 * @brief Sets the visibility of the Linux window.
	 * @param visible True to show, false to hide.
	 */
	void SetVisible(bool visible)
	{
		visibleFlag = visible;
		if (display && window)
		{
			XMapWindow(display, window);
		}
	}

	/**
	 * @brief Polls for X11 events.
	 */
	void PollEvents() override;

	/**
	 * @brief Checks if the Linux window should be closed.
	 * @return True if a close event was received, false otherwise.
	 */
	[[nodiscard]] bool ShouldClose() const override;

	/**
	 * @brief Closes and destroys the Linux window resources.
	 */
	void Close() override;

private:
	Display* display = nullptr;
	Window window = 0;
	int width = 0;
	int height = 0;
	bool visibleFlag = true;
	bool shouldCloseFlag = false;
};

} // namespace OS
#endif // PLATFORM_LINUX
