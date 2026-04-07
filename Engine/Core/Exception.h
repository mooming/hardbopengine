// Created by mooming.go@gmail.com, 2017

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
