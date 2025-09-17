// Created by mooming.go@gmail.com

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
