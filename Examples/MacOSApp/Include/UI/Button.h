// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <functional>
#include <string>

#include "UI/UIComponent.h"

namespace hbe
{

/**
 * @brief A simple rectangular button with a text label.
 *
 * The button changes appearance on hover and press. When clicked
 * (mouse down + release within bounds), the onClicked callback fires.
 */
class Button final : public UIComponent
{
public:
	enum class Type
	{
		Normal,  // Standard button
		Danger,  // Red-themed (e.g., Close)
		Primary, // Blue-themed (e.g., Full Screen)
	};

	Button();
	~Button() override = default;

	Button(const Button&) = delete;
	Button& operator=(const Button&) = delete;
	Button(Button&&) noexcept = delete;
	Button& operator=(Button&&) noexcept = delete;

	/**
	 * @brief Sets the button label text.
	 * @param label The text to display on the button.
	 */
	void SetLabel(const std::string& label) noexcept;

	/**
	 * @brief Sets the button type (affects color scheme).
	 * @param type The button type.
	 */
	void SetType(Type type) noexcept;

	/**
	 * @brief Registers a callback invoked when the button is clicked.
	 * @param callback The callback function.
	 */
	void SetOnClick(std::function<void()> callback) noexcept;

	[[nodiscard]] const std::string& GetLabel() const noexcept;
	[[nodiscard]] Type GetType() const noexcept;
	[[nodiscard]] bool IsPressed() const noexcept;

	void OnUpdate(const InputState& input) override;
	void Draw(Framebuffer& framebuffer) override;

private:
	std::string label;
	Type buttonType;
	bool pressedFlag;
	bool wasPressedOutside;  // Track if mouse was pressed outside, to cancel click

	// Colors (ARGB format)
	static constexpr uint32_t NormalBg = 0xFF2D3139u;
	static constexpr uint32_t NormalHoverBg = 0xFF3A3F4Au;
	static constexpr uint32_t NormalPressedBg = 0xFF1F2428u;
	static constexpr uint32_t NormalBorderColor = 0xFF30363Du;
	static constexpr uint32_t DangerBg = 0xFFf85149u;
	static constexpr uint32_t DangerHoverBg = 0xFFff6b6b;
	static constexpr uint32_t DangerPressedBg = 0xFFcc3333u;
	static constexpr uint32_t DangerBorderColor = 0xFFcc3333u;
	static constexpr uint32_t PrimaryBg = 0xFF2D3139u;
	static constexpr uint32_t PrimaryHoverBg = 0xFF58A6FFu;
	static constexpr uint32_t PrimaryPressedBg = 0xFF3A7BC8u;
	static constexpr uint32_t PrimaryBorderColor = 0xFF58A6FFu;
	static constexpr uint32_t TextColor = 0xFFC9D1D9u;
	static constexpr uint32_t HoverTextColor = 0xFFFFFFFFu;
	static constexpr uint32_t PressedTextColor = 0xFFFFFFFFu;

	std::function<void()> onClicked;
};

} // namespace hbe
