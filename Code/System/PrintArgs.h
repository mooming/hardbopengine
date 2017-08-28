// Copyright Hansol Park (mooming.go@gmail.com). All rights reserved.

#ifndef PrintArgs_h
#define PrintArgs_h

#include <iostream>

namespace
{
    template <typename T>
    inline void PrintArgs(const T& arg)
    {
        std::cout << arg << std::endl;
    }

    template <typename T, typename ... Types>
    inline void PrintArgs(const T& arg, Types&& ... args)
    {
        std::cout << arg;

        PrintArgs(std::forward<Types>(args) ...);
    }
}

#endif /* PrintArgs_h */
