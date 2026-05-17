// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <iostream>

namespace
{

	template<typename T>
	static void PrintArgs(const T& arg) noexcept
	{
		std::cout << arg << std::endl;
	}

	template<typename T, typename... TTypes>
	static void PrintArgs(const T& arg, TTypes&&... args) noexcept
	{
		std::cout << arg;
		PrintArgs(std::forward<TTypes>(args)...);
	}

} // namespace
