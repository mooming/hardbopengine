// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.


#pragma once


#include "HSTL/HString.h"

namespace OS
{

/**
 * @brief Interface for a platform-independent window.
 */
class IWindow
{
public:
	virtual ~IWindow() = default;

	/**
	 * @brief Creates the window with specified title and dimensions.
	 * @param title The title of the window.
	 * @param width The initial width of the window in pixels.
	 * @param height The initial height of the window in pixels.
	 * @return True if the window was created successfully, false otherwise.
	 */
	virtual bool CreateWindow(const hbe::HString& title, int width, int height) = 0;

	/**
	 * @brief Sets the title of the window.
	 * @param title The new title for the window.
	 */
	virtual void SetTitle(const hbe::HString& title) = 0;

	/**
	 * @brief Resizes the window to specified dimensions.
	 * @param width The new width in pixels.
	 * @param height The new height in pixels.
	 */
	virtual void SetSize(int width, int height) = 0;

	/**
	 * @brief Sets whether the window is visible or hidden.
	 * @param visible True to show the window, false to hide it.
	 */
	virtual void SetVisible(bool visible) = 0;

	/**
	 * @brief Processes pending OS events (e.g., input, resizing).
	 */
	virtual void PollEvents() = 0;

	/**
	 * @brief Closes the window and cleans up its resources.
	 */
	virtual void Close() = 0;

	/**
	 * @brief Gets the current width of the window in pixels.
	 * @return The width of the window.
	 */
	[[nodiscard]] virtual int GetWidth() const = 0;

	/**
	 * @brief Gets the current height of the window in pixels.
	 * @return The height of the window.
	 */
	[[nodiscard]] virtual int GetHeight() const = 0;

	/**
	 * @brief Checks if the window is currently visible.
	 * @return True if visible, false otherwise.
	 */
	[[nodiscard]] virtual bool IsVisible() const = 0;

	/**
	 * @brief Checks if a close event has been triggered.
	 * @return True if the window should be closed, false otherwise.
	 */
	[[nodiscard]] virtual bool ShouldClose() const = 0;
};

// Factory function to create a window for the current platform
IWindow* CreateWindow(const hbe::HString& title, int width, int height);

} // namespace OS



