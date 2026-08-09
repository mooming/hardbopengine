// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "UI/ScrollBar.h"

#include "Core/CommonMacros.h"

#include "Framebuffer.h"


namespace hbe
{

namespace
{

constexpr int ThumbWidth = 16;
constexpr int TrackPadding = 2;
constexpr int MinThumbHeight = 16;

} // namespace


ScrollBar::ScrollBar()
	: minValue(0.0f)
	, maxValue(100.0f)
	, currentValue(0.0f)
	, pageSize(0.3f)
	, draggingThumb(false)
	, thumbDragStartY(0)
	, thumbDragStartValue(0)
	, thumbTopCache(0)
	, thumbHeightCache(0)
{
}

void ScrollBar::SetRange(float min, float max) noexcept
{
	minValue = min;
	maxValue = max;

	if (maxValue <= minValue)
	{
		maxValue = minValue + 1.0f;
	}

	// Clamp current value
	if (currentValue < minValue)
	{
		currentValue = minValue;
	}
	else if (currentValue > maxValue)
	{
		currentValue = maxValue;
	}
}

void ScrollBar::SetPageSize(float size) noexcept
{
	pageSize = size < 0.0f ? 0.0f : (size > 1.0f ? 1.0f : size);
}

void ScrollBar::SetValue(float value) noexcept
{
	if (value < minValue)
	{
		value = minValue;
	}
	else if (value > maxValue)
	{
		value = maxValue;
	}

	if (onValueChanged && value != currentValue)
	{
		currentValue = value;
		onValueChanged(currentValue);
	}
	else
	{
		currentValue = value;
	}
}

void ScrollBar::SetOnValueChanged(std::function<void(float)> callback) noexcept
{
	onValueChanged = std::move(callback);
}

float ScrollBar::GetValue() const noexcept
{
	return currentValue;
}

float ScrollBar::GetMinValue() const noexcept
{
	return minValue;
}

float ScrollBar::GetMaxValue() const noexcept
{
	return maxValue;
}

bool ScrollBar::IsDragging() const noexcept
{
	return draggingThumb;
}

void ScrollBar::OnUpdate(const InputState& input)
{
	if (!visibleFlag)
	{
		hoveredFlag = false;
		return;
	}

	// Calculate thumb position and cache it for use in Draw()
	const int trackX = x + (width - ThumbWidth) / 2;
	const int trackStartY = y + TrackPadding;
	const int trackEndY = y + height - TrackPadding;
	const int trackHeight = trackEndY - trackStartY;

	const float range = maxValue - minValue;
	const float thumbHeightF = static_cast<float>(trackHeight) * pageSize;
	const int thumbHeight = thumbHeightF < MinThumbHeight ? MinThumbHeight : static_cast<int>(thumbHeightF);

	const float valueRange = range - (range * pageSize);
	float thumbY = trackStartY;
	if (valueRange > 0.001f)
	{
		const float progress = (currentValue - minValue) / valueRange;
		thumbY = trackStartY + progress * static_cast<float>(trackHeight - thumbHeight);
	}

	thumbTopCache = static_cast<int>(thumbY);
	thumbHeightCache = thumbHeight;

	// Hit detection
	const bool thumbContainsMouse = input.mouseX >= trackX && input.mouseX < trackX + ThumbWidth
	                              && input.mouseY >= thumbTopCache && input.mouseY < thumbTopCache + thumbHeightCache;

	// Track if mouse is over the scrollbar (for hover visual)
	hoveredFlag = Contains(input.mouseX, input.mouseY);

	// Handle starting a drag
	if (input.leftMousePressed)
	{
		if (thumbContainsMouse)
		{
			draggingThumb = true;
			thumbDragStartY = input.mouseY;
			thumbDragStartValue = static_cast<int>(currentValue);
		}
		else if (Contains(input.mouseX, input.mouseY))
		{
			// Click on track — scroll by a page
			if (input.mouseY < thumbTopCache)
			{
				SetValue(currentValue - (range * pageSize * 0.2f));
			}
			else
			{
				SetValue(currentValue + (range * pageSize * 0.2f));
			}
		}
	}

	// Handle ongoing drag (mouse can move outside scrollbar during drag)
	if (draggingThumb)
	{
		returnIf(input.leftMouseReleased);

		const int dragDelta = input.mouseY - thumbDragStartY;
		if (valueRange > 0.001f)
		{
			const float deltaValue = static_cast<float>(dragDelta) /
				static_cast<float>(trackHeight - thumbHeight) * valueRange;
			SetValue(static_cast<float>(thumbDragStartValue) + deltaValue);
		}
	}

	if (input.leftMouseReleased && draggingThumb)
	{
		draggingThumb = false;
	}
}

void ScrollBar::Draw(Framebuffer& framebuffer)
{
	if (!visibleFlag)
	{
		return;
	}

	const int trackX = x + (width - ThumbWidth) / 2;
	const int trackStartY = y + TrackPadding;
	const int trackEndY = y + height - TrackPadding;
	const int trackHeight = trackEndY - trackStartY;

	// Draw track background
	framebuffer.FillRect(trackX, trackStartY, ThumbWidth, trackHeight, TrackColor);

	// Draw track border
	framebuffer.DrawLine(trackX, trackStartY, trackX + ThumbWidth - 1, trackStartY, TrackBorderColor);
	framebuffer.DrawLine(trackX, trackEndY - 1, trackX + ThumbWidth - 1, trackEndY - 1, TrackBorderColor);

	// Use cached thumb position from OnUpdate
	const int thumbTop = thumbTopCache;
	const int thumbHeight = thumbHeightCache;
	const uint32_t thumbColor = (hoveredFlag || draggingThumb) ? ThumbHoverColor : ThumbColor;

	// Draw thumb
	framebuffer.FillRect(trackX, thumbTop, ThumbWidth, thumbHeight, thumbColor);

	// Draw thumb border
	framebuffer.DrawLine(trackX, thumbTop, trackX + ThumbWidth - 1, thumbTop, TrackBorderColor);
	framebuffer.DrawLine(trackX, thumbTop + thumbHeight - 1, trackX + ThumbWidth - 1, thumbTop + thumbHeight - 1, TrackBorderColor);
}

} // namespace hbe
