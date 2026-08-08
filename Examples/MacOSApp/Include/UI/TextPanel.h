// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <functional>
#include <string>
#include <vector>

#include "UI/UIComponent.h"

namespace hbe
{

/**
 * @brief A scrollable text display panel.
 *
 * Renders multiple lines of text using the framebuffer's bitmap font.
 * When the content exceeds the visible area, a scrollbar can be attached
 * to control the scroll offset.
 */
class TextPanel final : public UIComponent
{
public:
	using ScrollCallback = std::function<void(float)>;

	TextPanel();
	~TextPanel() override = default;

	TextPanel(const TextPanel&) = delete;
	TextPanel& operator=(const TextPanel&) = delete;
	TextPanel(TextPanel&&) noexcept = delete;
	TextPanel& operator=(TextPanel&&) noexcept = delete;

	/**
	 * @brief Sets the text content. Lines are split on newline characters.
	 * @param text Multi-line text string.
	 */
	void SetText(const std::string& text) noexcept;

	/**
	 * @brief Adds a line of text.
	 * @param line The line to append.
	 */
	void AddLine(const std::string& line) noexcept;

	/**
	 * @brief Registers a callback that is invoked when the scrollbar value changes.
	 * @param callback The callback function.
	 */
	void SetScrollCallback(ScrollCallback callback) noexcept;

	/**
	 * @brief Sets the current scroll offset (in lines).
	 * @param offset Line index to scroll to.
	 */
	void SetScrollOffset(int offset) noexcept;

	/**
	 * @brief Scrolls by a relative amount.
	 * @param delta Number of lines to scroll (positive = down).
	 */
	void ScrollBy(int delta) noexcept;

	[[nodiscard]] int GetScrollOffset() const noexcept;
	[[nodiscard]] int GetTotalLines() const noexcept;
	[[nodiscard]] int GetVisibleLineCount() const noexcept;

	void OnUpdate(const InputState& input) override;
	void Draw(Framebuffer& framebuffer) override;

private:
	std::vector<std::string> lines;
	int scrollOffset;
	int lineHeight;
	ScrollCallback scrollCallback;

	// Colors (ARGB format)
	static constexpr uint32_t BgColor = 0xFF161B22u;
	static constexpr uint32_t BorderColor = 0xFF30363Du;
	static constexpr uint32_t TextColor = 0xFFC9D1D9u;
	static constexpr uint32_t PaddingColor = 0xFF0D1117u;
};

} // namespace hbe
