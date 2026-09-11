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

void flushLogs();
} // namespace hbe

#ifdef __DEBUG__
namespace hbe
{

// Both sides of the __DEBUG__ guard below must declare exactly these two overloads, with the
// same exception specification. They used to differ: the release branch demanded a const char*
// as the second argument, so a message built from any other type compiled under __DEBUG__, ran
// for days, then broke the Release build. A noexcept mismatch alone is enough to flip
// std::is_nothrow_* traits between configurations, so both branches say noexcept.
inline void Assert(bool shouldBeTrue) noexcept
{
	if (likely(shouldBeTrue))
	{
		return;
	}

	flushLogs();
	printArgs("[Assert] Please check it.");

	debugBreak();
	std::abort();
}

template<typename... Types>
void Assert(bool shouldBeTrue, Types&&... args) noexcept
{
	if (likely(shouldBeTrue))
	{
		return;
	}

	flushLogs();
	printArgs("[Assert] ", std::forward<Types>(args)...);

	debugBreak();
	std::abort();
}

} // namespace hbe

#else // __DEBUG__

namespace hbe
{
// The arguments are unnamed and unused on purpose: the call inlines away, which also means an
// argument with a side effect is evaluated in Debug and dropped here. Same trap as any assert.
inline void Assert(bool) noexcept {}

template<typename... Types>
void Assert(bool, Types&&...) noexcept
{}
} // namespace hbe
#endif // __DEBUG__

namespace hbe
{
inline void fatalAssert(bool shouldBeTrue)
{
	if (likely(shouldBeTrue))
	{
		return;
	}

	flushLogs();
	printArgs("[FatalAssert] Please check it.");
	debugBreak();
	std::abort();
}

template<typename... Types>
void fatalAssert(bool shouldBeTrue, Types&&... args)
{
	if (likely(shouldBeTrue))
	{
		return;
	}

	flushLogs();
	printArgs("[FatalAssert] ", std::forward<Types>(args)...);
	debugBreak();
	std::abort();
}
} // namespace hbe
