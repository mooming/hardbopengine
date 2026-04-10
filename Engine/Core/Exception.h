// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstdio>
#include <utility>
#include "Log/PrintArgs.h"

namespace hbe
{
	/// @brief A simple exception class for reporting errors with file and line information.
	class Exception
	{
	public:
		template<typename... Types>
		inline Exception(const char* file, int line, Types&&... args)
		{
			using namespace std;
			PrintArgs(file, ":", line, forward<Types>(args)...);
		}
	};
} // namespace hbe
