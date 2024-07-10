// Created by mooming.go@gmail.com, 2017

#pragma once

#include "Log/PrintArgs.h"
#include <cstdio>
#include <utility>


namespace HE
{
class Exception
{
  public:
    template <typename... Types>
    inline Exception(const char* file, int line, Types&&... args)
    {
        using namespace std;
        PrintArgs(file, ":", line, forward<Types>(args)...);
    }
};
} // namespace HE
