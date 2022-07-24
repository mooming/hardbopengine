// Copyright Hansol Park (anav96@naver.com, mooming.go@gmail.com). All rights reserved.

#ifndef Exception_h
#define Exception_h

#include "PrintArgs.h"

#include <cstdio>
#include <utility>

namespace HE
{
    class Exception
    {
    public:
        template <typename ... Types>
        inline Exception(const char* file, int line, Types&& ... args)
        {
            using namespace std;
            PrintArgs(file, ":", line, forward<Types>(args) ...);

        }
    };
}

#endif /* Exception_h */
