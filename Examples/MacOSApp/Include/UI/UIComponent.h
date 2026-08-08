// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstdint>
#include <string>

namespace hbe
{

class Framebuffer;

/**
 * @brief Input state for UI components, captured from the window's mouse
 * and keyboard state each frame.
 */
struct InputState
{
	int mouseX = 0;
	int mouseY = 0;
	bool leftMousePressed = false;
	bool leftMouseReleased = false;
	float mouseWheelDeltaY = 0.0f;

	// Recent key press tracking (since last frame)
	bool f11Pressed = false;
	bool escapePressed = false;
};

/**
 * @brief Abstract base class for all UI components.
 *
 * UI components are drawn onto a Framebuffer and receive input events
 * for hit-testing and interaction. Each component manages its own position,
 * size, visibility, and hover state.
 */
class UIComponent
{
public:
	virtual ~UIComponent() = default;

	UIComponent();

	UIComponent(const UIComponent&) = delete;
	UIComponent& operator=(const UIComponent&) = delete;
	UIComponent(UIComponent&&) noexcept = delete;
	UIComponent& operator=(UIComponent&&) noexcept = delete;

	/**
	 * @brief Sets the position and size of this component.
	 * @param inX Top-left X coordinate.
	 * @param inY Top-left Y coordinate.
	 * @param inW Width in pixels.
	 * @param inH Height in pixels.
	 */
	void SetBounds(int inX, int inY, int inW, int inH) noexcept;

	/**
	 * @brief Sets the position (top-left corner) of this component.
	 * @param inX X coordinate.
	 * @param inY Y coordinate.
	 */
	void SetPosition(int inX, int inY) noexcept;

	/**
	 * @brief Sets the size of this component.
	 * @param inW Width in pixels.
	 * @param inH Height in pixels.
	 */
	void SetSize(int inW, int inH) noexcept;

	/**
	 * @brief Sets visibility.
	 * @param visible True to make visible, false to hide.
	 */
	void SetVisible(bool visible) noexcept;

	/**
	 * @brief Checks if a point falls within this component's bounds.
	 * @param px X coordinate to test.
	 * @param py Y coordinate to test.
	 * @return True if the point is inside the component rectangle.
	 */
	[[nodiscard]] bool Contains(int px, int py) const noexcept;

	/**
	 * @brief Checks if the mouse cursor is currently over this component.
	 * @return True if hovered.
	 */
	[[nodiscard]] bool IsHovered() const noexcept;

	[[nodiscard]] int GetX() const noexcept;
	[[nodiscard]] int GetY() const noexcept;
	[[nodiscard]] int GetWidth() const noexcept;
	[[nodiscard]] int GetHeight() const noexcept;
	[[nodiscard]] bool IsVisible() const noexcept;

	/**
	 * @brief Called once per frame with the current input state.
	 * Components should update their internal state and check for
	 * interactions (clicks, hovers, drags) here.
	 * @param input The current input state.
	 */
	virtual void OnUpdate(const InputState& input) = 0;

	/**
	 * @brief Draws the component onto the framebuffer.
	 * @param framebuffer The target framebuffer.
	 */
	virtual void Draw(Framebuffer& framebuffer) = 0;

protected:
	int x;
	int y;
	int width;
	int height;
	bool visibleFlag;
	bool hoveredFlag;
};

} // namespace hbe
