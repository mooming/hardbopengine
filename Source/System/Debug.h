// Created by mooming.go@gmail.com, 2017 ~ 2022

#pragma once

#include "Config/EngineConfig.h"
#include "Log/PrintArgs.h"
#include "OSAL/Intrinsic.h"


namespace HE
{
template <typename T>
using TDebugVariable = const T;

void FlushLogs();
} // HE

#ifdef __DEBUG__
#include <cstdlib>
#include <cstdio>
#include <memory>


namespace HE
{

inline void Assert(bool shouldBeTrue)
{
    if (likely(shouldBeTrue))
        return;

    FlushLogs();
    PrintArgs("[Assert] Please check it.");

    debugBreak();
    std::abort();
}

template <typename ... Types>
inline void Assert(bool shouldBeTrue, Types&& ... args)
{
    if (likely(shouldBeTrue))
        return;

    FlushLogs();
    PrintArgs("[Assert] ", std::forward<Types>(args) ...);

    debugBreak();
    std::abort();
}
} // HE

#else // __DEBUG__
#include <cstdlib>
#include <cstdio>
#include <memory>


namespace HE
{
inline void Assert(bool)
{
}

template <typename ... Types>
inline void Assert(bool, const char*, Types&& ...)
{
}
} // HE
#endif // __DEBUG__

namespace HE
{
inline void FatalAssert(bool shouldBeTrue)
{
    if (likely(shouldBeTrue))
        return;

    FlushLogs();
    PrintArgs("[FatalAssert] Please check it.");
    debugBreak();
    std::abort();
}

template <typename ... Types>
inline void FatalAssert(bool shouldBeTrue, Types&& ... args)
{
    if (likely(shouldBeTrue))
        return;

    FlushLogs();
    PrintArgs("[FatalAssert] ", std::forward<Types>(args) ...);
    debugBreak();
    std::abort();
}
} // HE
