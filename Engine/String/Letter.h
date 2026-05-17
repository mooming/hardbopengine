// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

namespace hbe
{
	/// @brief Utility class for character classification and validation.
	class Letter
	{
	public:
		using TChar = char;

		static bool IsLowerCase(TChar letter)
		{
			if (letter < 'a')
				return false;
			if (letter > 'z')
				return false;

			return true;
		}

		static bool IsUpperCase(TChar letter)
		{
			if (letter < 'A')
				return false;
			if (letter > 'Z')
				return false;

			return true;
		}

		static bool IsGenuineLetter(TChar letter)
		{
			if (letter == ' ' || letter == '\t' || letter == '\n' || letter == '\r')
				return false;

			return true;
		}
	};
} // namespace hbe
