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
		returnValueIf(false, letter < 'a');
		returnValueIf(false, letter > 'z');

		return true;
	}

	[[nodiscard]] static bool IsUpperCase(TChar letter)
	{
		returnValueIf(false, letter < 'A');
		returnValueIf(false, letter > 'Z');

		return true;
	}

	[[nodiscard]] static bool IsNotWhiteSpace(TChar letter)
	{
		returnValueIf(false, letter == ' ' || letter == '\t' || letter == '\n' || letter == '\r');

		return true;
	}
};
} // namespace hbe
