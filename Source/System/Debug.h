// Created by mooming.go@gmail.com, 2017

#pragma once

#include "Config/EngineConfig.h"
#include "Log/PrintArgs.h"
#include "OSAL/Intrinsic.h"

#ifdef __DEBUG__

#include <cstdlib>
#include <cstdio>
#include <memory>

namespace HE
{
    template <typename T>
    using TDebugVariable = volatile T;

    inline void Assert(bool shouldBeTrue)
    {
        if (likely(shouldBeTrue))
            return;

        PrintArgs("[Assert] Please check it.");
        
        debugBreak();
        std::abort();
    }

    template <typename ... Types>
    inline void Assert(bool shouldBeTrue, Types&& ... args)
    {
        if (likely(shouldBeTrue))
            return;

        PrintArgs("[Assert] ", std::forward<Types>(args) ...);
        
        debugBreak();
        std::abort();
    }
}

#else // __DEBUG__

#include <cstdlib>
#include <cstdio>
#include <memory>

namespace HE
{
    template <typename T>
    using TDebugVariable = const T;

    inline void Assert(bool)
    {
    }

    template <typename ... Types>
    inline void Assert(bool, const char*, Types&& ...)
    {
    }
}
#endif // __DEBUG__

namespace HE
{
    inline void FatalAssert(bool shouldBeTrue)
    {
        if (likely(shouldBeTrue))
            return;

        PrintArgs("[FatalAssert] Please check it.");
        debugBreak();
        std::abort();
    }

    template <typename ... Types>
    inline void FatalAssert(bool shouldBeTrue, Types&& ... args)
    {
        if (likely(shouldBeTrue))
            return;

        PrintArgs("[FatalAssert] ", std::forward<Types>(args) ...);
        debugBreak();
        std::abort();
    }
} // anonymous
