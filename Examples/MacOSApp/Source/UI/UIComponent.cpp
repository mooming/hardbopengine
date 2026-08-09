// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "UI/UIComponent.h"


namespace hbe
{

UIComponent::UIComponent()
	: x(0)
	, y(0)
	, width(0)
	, height(0)
	, visibleFlag(true)
	, hoveredFlag(false)
{
}

void UIComponent::SetBounds(int inX, int inY, int inW, int inH) noexcept
{
	x = inX;
	y = inY;
	width = inW;
	height = inH;
}

void UIComponent::SetPosition(int inX, int inY) noexcept
{
	x = inX;
	y = inY;
}

void UIComponent::SetSize(int inW, int inH) noexcept
{
	width = inW;
	height = inH;
}

void UIComponent::SetVisible(bool visible) noexcept
{
	visibleFlag = visible;
}

bool UIComponent::Contains(int px, int py) const noexcept
{
	return px >= x && px < x + width && py >= y && py < y + height;
}

bool UIComponent::IsHovered() const noexcept
{
	return hoveredFlag;
}

int UIComponent::GetX() const noexcept
{
	return x;
}

int UIComponent::GetY() const noexcept
{
	return y;
}

int UIComponent::GetWidth() const noexcept
{
	return width;
}

int UIComponent::GetHeight() const noexcept
{
	return height;
}

bool UIComponent::IsVisible() const noexcept
{
	return visibleFlag;
}

} // namespace hbe
