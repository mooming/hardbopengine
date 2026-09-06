// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstdio>
#include <cstdlib>
#include <memory>

#include "Log/PrintArgs.h"
#include "OSAL/Intrinsic.h"

// The three standard headers above are used only by the __DEBUG__ branch below, but they are
// included unconditionally on purpose: the project's include layout puts every include in one
// block at the top of the file, and a guarded #include cannot satisfy it (see JOURNAL.md).

namespace hbe
{
template<typename T>
using TDebugVariable = const T;

void FlushLogs();
} // namespace hbe

#ifdef __DEBUG__
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
void Assert(bool shouldBeTrue, Types&&... args)
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
inline void Assert(bool) noexcept {}

template<typename... Types>
void Assert(bool, const char*, Types&&...) noexcept
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
void FatalAssert(bool shouldBeTrue, Types&&... args)
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
