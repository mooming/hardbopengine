// Created by mooming.go@gmail.com

#pragma once

#ifdef __clang__
#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)
#define debugBreak() __builtin_trap()

#define CallerFile() __builtin_FILE()
#define CallerFunc() __builtin_FUNCTION()
#define CallerLine() __builtin_LINE()
#define CallerColumn() __builtin_COLUMN()

#elif __GNUC__
#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)
#define debugBreak() __builtin_trap()

#define CallerFile() __builtin_FILE()
#define CallerFunc() __builtin_FUNCTION()
#define CallerLine() __builtin_LINE()
#define CallerColumn() __builtin_COLUMN()

#elif _MSC_VER
#include <intrin.h>

#define likely(x) x
#define unlikely(x) x
#define debugBreak() __debugbreak()

#define CallerFile() __builtin_FILE()
#define CallerFunc() __builtin_FUNCTION()
#define CallerLine() __builtin_LINE()
#define CallerColumn() __builtin_COLUMN()

#define __PRETTY_FUNCTION__ __FUNCSIG__

#endif // __clang__
