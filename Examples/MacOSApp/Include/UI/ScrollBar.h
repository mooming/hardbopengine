// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <functional>

#include "UI/UIComponent.h"

namespace hbe
{

/**
 * @brief A vertical scrollbar component.
 *
 * Displays a track with a draggable thumb. The scroll value ranges from
 * minValue to maxValue. When the user drags the thumb or clicks the track,
 * the onValueChanged callback is invoked to notify listeners.
 */
class ScrollBar final : public UIComponent
{
public:
	ScrollBar();
	~ScrollBar() override = default;

	ScrollBar(const ScrollBar&) = delete;
	ScrollBar& operator=(const ScrollBar&) = delete;
	ScrollBar(ScrollBar&&) noexcept = delete;
	ScrollBar& operator=(ScrollBar&&) noexcept = delete;

	/**
	 * @brief Sets the scroll range.
	 * @param min The minimum scroll value.
	 * @param max The maximum scroll value.
	 */
	void SetRange(float min, float max) noexcept;

	/**
	 * @brief Sets the visible page size relative to the total range.
	 * @param pageSize The page size as a fraction of the range (0.0 to 1.0).
	 */
	void SetPageSize(float pageSize) noexcept;

	/**
	 * @brief Sets the current scroll value.
	 * @param value The new scroll value.
	 */
	void SetValue(float value) noexcept;

	/**
	 * @brief Registers a callback invoked when the scroll value changes.
	 * @param callback The callback function receiving the new value.
	 */
	void SetOnValueChanged(std::function<void(float)> callback) noexcept;

	[[nodiscard]] float GetValue() const noexcept;
	[[nodiscard]] float GetMinValue() const noexcept;
	[[nodiscard]] float GetMaxValue() const noexcept;
	[[nodiscard]] bool IsDragging() const noexcept;

	void OnUpdate(const InputState& input) override;
	void Draw(Framebuffer& framebuffer) override;

private:
	float minValue;
	float maxValue;
	float currentValue;
	float pageSize;
	bool draggingThumb;
	int thumbDragStartY;
	int thumbDragStartValue;
	int thumbTopCache;
	int thumbHeightCache;

	// Colors (ARGB format)
	static constexpr uint32_t TrackColor = 0xFF30363Du;
	static constexpr uint32_t ThumbColor = 0xFF58A6FFu;
	static constexpr uint32_t ThumbHoverColor = 0xFF75B7FFu;
	static constexpr uint32_t TrackBorderColor = 0xFF0D1117u;

	void UpdateThumbRect();

	std::function<void(float)> onValueChanged;
};

} // namespace hbe
