// Copyright Hansol Park (anav96@naver.com, mooming.go@gmail.com). All rights reserved.

#ifndef Debug_h
#define Debug_h

#include "Platform.h"
#include "PrintArgs.h"

#ifdef __DEBUG__

#include <cstdlib>
#include <cstdio>
#include <memory>

namespace
{
	inline void Assert(bool shouldBeTrue)
	{
		if (!shouldBeTrue)
		{
			std::abort();
		}
	}

	template <typename ... Types>
	inline void AssertMessage(bool shouldBeTrue, Types&& ... args)
	{
		if (!shouldBeTrue)
		{
            PrintArgs(std::forward<Types>(args) ...);
			std::abort();
		}
	}
}

#else // __DEBUG__

#include <cstdlib>
#include <cstdio>
#include <memory>

namespace
{
    inline void Assert(bool)
    {
    }

    template <typename ... Types>
    inline void AssertMessage(bool, const char*, Types&& ...)
    {
    }
}
#endif // __DEBUG__

namespace
{
	inline void FatalAssert(bool shouldBeTrue)
	{
		if (!shouldBeTrue)
		{
			std::abort();
		}
	}

	template <typename ... Types>
	inline void FatalAssertMessage(bool shouldBeTrue, Types&& ... args)
	{
		if (!shouldBeTrue)
		{
            PrintArgs(std::forward<Types>(args) ...);
			std::abort();
		}
	}
}

#endif /* Debug_h */
