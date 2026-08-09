// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "UI/Button.h"

#include "Framebuffer.h"


namespace hbe
{

Button::Button()
	: buttonType(Type::Normal)
	, pressedFlag(false)
	, wasPressedOutside(false)
{
}

void Button::SetLabel(const std::string& inLabel) noexcept
{
	label = inLabel;
}

void Button::SetType(Type type) noexcept
{
	buttonType = type;
}

void Button::SetOnClick(std::function<void()> callback) noexcept
{
	onClicked = std::move(callback);
}

const std::string& Button::GetLabel() const noexcept
{
	return label;
}

Button::Type Button::GetType() const noexcept
{
	return buttonType;
}

bool Button::IsPressed() const noexcept
{
	return pressedFlag;
}

void Button::OnUpdate(const InputState& input)
{
	if (!visibleFlag)
	{
		return;
	}

	hoveredFlag = Contains(input.mouseX, input.mouseY);

	if (input.leftMousePressed)
	{
		if (hoveredFlag)
		{
			pressedFlag = true;
			wasPressedOutside = false;
		}
		else
		{
			wasPressedOutside = true;
		}
	}

	if (pressedFlag && input.leftMouseReleased)
	{
		if (hoveredFlag && !wasPressedOutside)
		{
			pressedFlag = false;
			if (onClicked)
			{
				onClicked();
			}
		}
		else
		{
			pressedFlag = false;
		}
		wasPressedOutside = false;
	}
}

void Button::Draw(Framebuffer& framebuffer)
{
	if (!visibleFlag)
	{
		return;
	}

	uint32_t bgColor;
	uint32_t borderColor;
	uint32_t textColor;

	switch (buttonType)
	{
		case Type::Danger:
			if (pressedFlag)
			{
				bgColor = DangerPressedBg;
				borderColor = DangerBorderColor;
			}
			else if (hoveredFlag)
			{
				bgColor = DangerHoverBg;
				borderColor = DangerBorderColor;
			}
			else
			{
				bgColor = DangerBg;
				borderColor = DangerBorderColor;
			}
			textColor = TextColor;
			break;

		case Type::Primary:
			if (pressedFlag)
			{
				bgColor = PrimaryPressedBg;
				borderColor = PrimaryBorderColor;
			}
			else if (hoveredFlag)
			{
				bgColor = PrimaryHoverBg;
				borderColor = PrimaryBorderColor;
			}
			else
			{
				bgColor = PrimaryBg;
				borderColor = PrimaryBorderColor;
			}
			textColor = hoveredFlag ? HoverTextColor : TextColor;
			break;

		case Type::Normal:
		default:
			if (pressedFlag)
			{
				bgColor = NormalPressedBg;
				borderColor = NormalBorderColor;
				textColor = PressedTextColor;
			}
			else if (hoveredFlag)
			{
				bgColor = NormalHoverBg;
				borderColor = NormalBorderColor;
				textColor = HoverTextColor;
			}
			else
			{
				bgColor = NormalBg;
				borderColor = NormalBorderColor;
				textColor = TextColor;
			}
			break;
	}

	// Draw button background
	framebuffer.FillRect(x, y, width, height, bgColor);

	// Draw button border
	framebuffer.DrawLine(x, y, x + width - 1, y, borderColor);
	framebuffer.DrawLine(x, y + height - 1, x + width - 1, y + height - 1, borderColor);
	framebuffer.DrawLine(x, y, x, y + height - 1, borderColor);
	framebuffer.DrawLine(x + width - 1, y, x + width - 1, y + height - 1, borderColor);

	// Draw label text (centered)
	const int charWidth = 9;
	const int textWidth = static_cast<int>(label.size()) * charWidth;
	const int textX = x + (width - textWidth) / 2;
	const int textY = y + (height - 12) / 2;  // 12 = font height

	framebuffer.DrawText(textX, textY, label.c_str(), textColor);
}

} // namespace hbe
