// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "Core/CommonMacros.h"


namespace hbe
{
/// API reference: docs/String/Letter/index.html
class Letter
{
public:
	using TChar = char;

	[[nodiscard]] static bool IsLowerCase(TChar letter)
	{
		ReturnValueIf(false, letter < 'a');
		ReturnValueIf(false, letter > 'z');

		return true;
	}

	[[nodiscard]] static bool IsUpperCase(TChar letter)
	{
		ReturnValueIf(false, letter < 'A');
		ReturnValueIf(false, letter > 'Z');

		return true;
	}

	[[nodiscard]] static bool IsNotWhiteSpace(TChar letter)
	{
		ReturnValueIf(false, letter == ' ' || letter == '\t' || letter == '\n' || letter == '\r');

		return true;
	}
};
} // namespace hbe
