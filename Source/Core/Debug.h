// Created by mooming.go@gmail.com, 2017 ~ 2022

#pragma once

#include "Log/PrintArgs.h"
#include "OSAL/Intrinsic.h"

namespace hbe
{
	template<typename T>
	using TDebugVariable = const T;

	void FlushLogs();
} // namespace hbe

#ifdef __DEBUG__
#include <cstdio>
#include <cstdlib>
#include <memory>

namespace hbe
{

	inline void Assert(bool shouldBeTrue)
	{
		if (likely(shouldBeTrue))
		{
			return;
		}

		FlushLogs();
		PrintArgs("[Assert] Please check it.");

		debugBreak();
		std::abort();
	}

	template<typename... Types>
	inline void Assert(bool shouldBeTrue, Types&&... args)
	{
		if (likely(shouldBeTrue))
		{
			return;
		}

		FlushLogs();
		PrintArgs("[Assert] ", std::forward<Types>(args)...);

		debugBreak();
		std::abort();
	}
} // namespace hbe

#else // __DEBUG__

namespace hbe
{
	inline void Assert(bool) {}

	template<typename... Types>
	inline void Assert(bool, const char*, Types&&...)
	{}
} // namespace hbe
#endif // __DEBUG__

namespace hbe
{
	inline void FatalAssert(bool shouldBeTrue)
	{
		if (likely(shouldBeTrue))
		{
			return;
		}

		FlushLogs();
		PrintArgs("[FatalAssert] Please check it.");
		debugBreak();
		std::abort();
	}

	template<typename... Types>
	inline void FatalAssert(bool shouldBeTrue, Types&&... args)
	{
		if (likely(shouldBeTrue))
		{
			return;
		}

		FlushLogs();
		PrintArgs("[FatalAssert] ", std::forward<Types>(args)...);
		debugBreak();
		std::abort();
	}
} // namespace hbe
