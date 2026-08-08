// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "UI/TextPanel.h"

#include <algorithm>

#include "Framebuffer.h"

namespace hbe
{

namespace
{

constexpr int CharWidth = 9;  // 8px font + 1px spacing
constexpr int FontHeight = 12;
constexpr int LineGap = 2;

} // namespace


TextPanel::TextPanel()
	: scrollOffset(0)
	, lineHeight(FontHeight + LineGap)
{
}

void TextPanel::SetText(const std::string& text) noexcept
{
	lines.clear();
	scrollOffset = 0;

	size_t start = 0;
	size_t end = text.find('\n');

	while (end != std::string::npos)
	{
		lines.emplace_back(text.substr(start, end - start));
		start = end + 1;
		end = text.find('\n', start);
	}

	// Add the last line
	if (start < text.size())
	{
		lines.emplace_back(text.substr(start));
	}
}

void TextPanel::AddLine(const std::string& line) noexcept
{
	lines.push_back(line);
}

void TextPanel::SetScrollCallback(ScrollCallback callback) noexcept
{
	scrollCallback = std::move(callback);
}

void TextPanel::SetScrollOffset(int offset) noexcept
{
	const int maxOffset = std::max(0, GetTotalLines() - GetVisibleLineCount());
	scrollOffset = offset < 0 ? 0 : (offset > maxOffset ? maxOffset : offset);
}

void TextPanel::ScrollBy(int delta) noexcept
{
	SetScrollOffset(scrollOffset + delta);
}

int TextPanel::GetScrollOffset() const noexcept
{
	return scrollOffset;
}

int TextPanel::GetTotalLines() const noexcept
{
	return static_cast<int>(lines.size());
}

int TextPanel::GetVisibleLineCount() const noexcept
{
	if (lineHeight == 0)
	{
		return 0;
	}

	const int contentHeight = height - 8; // 4px top + 4px bottom padding
	return std::max(1, contentHeight / lineHeight);
}

void TextPanel::OnUpdate(const InputState& input)
{
	if (!visibleFlag)
	{
		return;
	}

	hoveredFlag = Contains(input.mouseX, input.mouseY);
}

void TextPanel::Draw(Framebuffer& framebuffer)
{
	if (!visibleFlag)
	{
		return;
	}

	const int panelX = x;
	const int panelY = y;
	const int panelW = width;
	const int panelH = height;

	// Draw background
	framebuffer.FillRect(panelX, panelY, panelW, panelH, BgColor);

	// Draw border
	framebuffer.DrawLine(panelX, panelY, panelX + panelW - 1, panelY, BorderColor);
	framebuffer.DrawLine(panelX, panelY + panelH - 1, panelX + panelW - 1, panelY + panelH - 1, BorderColor);
	framebuffer.DrawLine(panelX, panelY, panelX, panelY + panelH - 1, BorderColor);
	framebuffer.DrawLine(panelX + panelW - 1, panelY, panelX + panelW - 1, panelY + panelH - 1, BorderColor);

	// Draw text content
	const int textX = panelX + 4;
	const int textY = panelY + 4;
	const int visibleLines = GetVisibleLineCount();
	const int maxVisibleX = textX + panelW - 8;

	for (int i = 0; i < visibleLines; ++i)
	{
		const int lineIndex = scrollOffset + i;
		if (lineIndex >= GetTotalLines())
		{
			break;
		}

		const std::string& line = lines[lineIndex];
		int currentX = textX;

		for (char ch : line)
		{
			if (currentX + CharWidth > maxVisibleX)
			{
				break;
			}

			framebuffer.DrawChar(currentX, textY + i * lineHeight, ch, TextColor);
			currentX += CharWidth;
		}
	}
}

} // namespace hbe
