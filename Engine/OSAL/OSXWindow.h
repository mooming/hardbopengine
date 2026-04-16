// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.


#pragma once


#include "Config/BuildConfig.h"

#ifdef PLATFORM_OSX
#include "Window.h"

namespace OS
{

/**
 * @brief OSX Window class implementation using Cocoa API.
 */
class OSXWindow : public IWindow
{
public:
	/**
	 * @brief Default constructor for OSXWindow.
	 */
	OSXWindow();

	/**
	 * @brief Virtual destructor for OSXWindow.
	 */
	~OSXWindow() override;

	/**
	 * @brief Creates a window on the OSX platform using Cocoa API.
	 * @param title The title of the window.
	 * @param width The initial width in pixels.
	 * @param height The initial height in pixels.
	 * @return True if successful, false otherwise.
	 */
	bool CreateWindow(const hbe::HString& title, int width, int height) override;

	/**
	 * @brief Sets the window title on OSX via Cocoa API.
	 * @param title The new title for the window.
	 */
	void SetTitle(const hbe::HString& title) override;

	/**
	 * @brief Resizes the OSX window to specified dimensions.
	 * @param width New width in pixels.
	 * @param height New height in pixels.
	 */
	void SetSize(int width, int height) override;

	/**
	 * @brief Gets the current width of the OSX window.
	 * @return The width in pixels.
	 */
	[[nodiscard]] int GetWidth() const override;

	/**
	 * @brief Gets the current height of the OSX window.
	 * @return The height in pixels.
	 */
	[[nodiscard]] int GetHeight() const override;

	/**
	 * @brief Checks if the OSX window is visible.
	 * @return True if visible, false otherwise.
	 */
	[[nodiscard]] bool IsVisible() const override;

	/**
	 * @brief Sets the visibility of the OSX window.
	 * @param visible True to show, false to hide.
	 */
	void SetVisible(bool visible) override;

	/**
	 * @brief Polls for Cocoa events via NSEvent.
	 */
	void PollEvents() override;

	/**
	 * @brief Checks if the OSX window should be closed.
	 * @return True if a close event was received, false otherwise.
	 */
	[[nodiscard]] bool ShouldClose() const override;

	/**
	 * @brief Closes and destroys the OSX window resources.
	 */
	void Close() override;

private:
	void* nsWindow = nullptr;
	int width = 0;
	int height = 0;
	bool visibleFlag = true;
	bool shouldCloseFlag = false;
};

} // namespace OS
#endif // PLATFORM_OSX
