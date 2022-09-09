// Created by mooming.go@gmail.com, 2017

#pragma once

#ifdef __clang__

#define likely(x) __builtin_expect((x),1)
#define unlikely(x) __builtin_expect((x),0)
#define debugBreak() __builtin_trap()

#elif __GNUC__

#define likely(x) __builtin_expect((x),1)
#define unlikely(x) __builtin_expect((x),0)
#define debugBreak() __builtin_trap()

#elif _MSC_VER

#include <intrin.h>


#define likely(x) x
#define unlikely(x) x
#define debugBreak() __debugbreak()

#define __PRETTY_FUNCTION__ __FUNCSIG__
#elif __BORLANDC__

#define likely(x) __builtin_expect((x),1)
#define unlikely(x) __builtin_expect((x),0)
#define debugBreak() __builtin_trap()

#elif __MINGW32__

#define likely(x) __builtin_expect((x),1)
#define unlikely(x) __builtin_expect((x),0)
#define debugBreak() __builtin_trap()

#endif // __clang__
