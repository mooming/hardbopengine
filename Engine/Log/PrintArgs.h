// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <iostream>

namespace
{

	template<typename T>
	inline static void PrintArgs(const T& arg)
	{
		std::cout << arg << std::endl;
	}

	template<typename T, typename... Types>
	inline static void PrintArgs(const T& arg, Types&&... args)
	{
		std::cout << arg;
		PrintArgs(std::forward<Types>(args)...);
	}

} // namespace
