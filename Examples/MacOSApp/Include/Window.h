// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace hbe
{

class Framebuffer;

/**
 * @brief Mouse input state captured from the latest events.
 */
struct MouseState
{
	int x = 0;
	int y = 0;
	bool leftButtonPressed = false;
	bool leftButtonReleased = false;
	float wheelDeltaY = 0.0f;
};

/**
 * @brief Manages a macOS window (NSWindow) with a software framebuffer.
 *
 * This class is a C++ wrapper around NSWindow, providing full-screen/windowed
 * mode toggling, framebuffer presentation, and input event capture. It replaces
 * the engine's OS::IWindow abstraction as a learning exercise.
 *
 * The implementation lives in Window.mm (Objective-C++).
 */
class Window final
{
public:
	static const int KeyF11 = 111;
	static const int KeyEscape = 53;

	Window();
	~Window();

	Window(const Window&) = delete;
	Window& operator=(const Window&) = delete;
	Window(Window&&) noexcept = delete;
	Window& operator=(Window&&) noexcept = delete;

	/**
	 * @brief Factory method to create a window with the given title and dimensions.
	 * @param title Window title text.
	 * @param width Initial width in pixels.
	 * @param height Initial height in pixels.
	 * @return Unique pointer to the created window, or nullptr on failure.
	 */
	[[nodiscard]] static std::unique_ptr<Window> Create(const std::string& title, int width, int height);

	/**
	 * @brief Sets the window title.
	 * @param title New title string.
	 */
	void SetTitle(const std::string& title) noexcept;

	/**
	 * @brief Resizes the window to the specified dimensions (windowed mode).
	 * @param inWidth New width in pixels.
	 * @param inHeight New height in pixels.
	 */
	void SetSize(int inWidth, int inHeight) noexcept;

	/**
	 * @brief Toggles between full-screen and windowed mode.
	 */
	void ToggleFullScreen() noexcept;

	/**
	 * @brief Processes pending OS events (mouse, keyboard, resize).
	 * Updates the mouse state and tracks recently pressed keys.
	 */
	void PollEvents() noexcept;

	/**
	 * @brief Presents the framebuffer contents to the window's content view.
	 * @param framebuffer The framebuffer to display.
	 */
	void Present(const Framebuffer& framebuffer) noexcept;

	/**
	 * @brief Closes the window and releases native resources.
	 */
	void Close() noexcept;

	/**
	 * @brief Checks if a specific key was pressed since the last PollEvents call.
	 * @param keyCode The macOS hardware key code to check.
	 * @return True if the key was pressed since the last call.
	 */
	[[nodiscard]] bool WasKeyPressed(int keyCode) const noexcept;

	/**
	 * @brief Gets the current mouse state.
	 * @return Reference to the latest mouse state.
	 */
	[[nodiscard]] const MouseState& GetMouseState() const noexcept;

	// Getters
	[[nodiscard]] int GetWidth() const noexcept;
	[[nodiscard]] int GetHeight() const noexcept;
	[[nodiscard]] bool IsVisible() const noexcept;
	[[nodiscard]] bool IsClosed() const noexcept;
	[[nodiscard]] bool IsFullScreen() const noexcept;
	[[nodiscard]] void* GetNativeHandle() const noexcept;

private:
	void* nsWindowHandle;
	void* nsViewHandle;
	int width;
	int height;
	bool visibleFlag;
	bool closedFlag;
	bool fullScreenFlag;
	bool lastFrameKeyStates[256];
	bool currentKeyStates[256];
	MouseState mouseState;
};

} // namespace hbe
