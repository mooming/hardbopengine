// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "Core/CommonMacros.h"

namespace hbe
{
	/// @brief Utility class for character classification and validation.
	class Letter
	{
	public:
		using TChar = char;

		static bool IsLowerCase(TChar letter)
		{
			returnValueIf(false, letter < 'a');
			returnValueIf(false, letter > 'z');

			return true;
		}

		static bool IsUpperCase(TChar letter)
		{
			returnValueIf(false, letter < 'A');
			returnValueIf(false, letter > 'Z');

			return true;
		}

		static bool IsGenuineLetter(TChar letter)
		{
			returnValueIf(false, letter == ' ' || letter == '\t' || letter == '\n' || letter == '\r');

			return true;
		}
	};
} // namespace hbe
