// Created by mooming.go@gmail.com, 2017

#pragma once

#include <iostream>

namespace
{
template <typename T>
inline void PrintArgs(const T& arg)
{
    std::cout << arg << std::endl;
}

template <typename T, typename... Types>
inline void PrintArgs(const T& arg, Types&&... args)
{
    std::cout << arg;
    PrintArgs(std::forward<Types>(args)...);
}
} // namespace
