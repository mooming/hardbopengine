// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <source_location>

#include "Log/PrintArgs.h"
#include "OSAL/Intrinsic.h"


namespace hbe
{
/// API reference: docs/Core/index.html#debug
template <typename T>
using TDebugVariable = const T;

void FlushLogs();
} // namespace hbe

#ifdef __DEBUG__
namespace hbe
{
inline void Assert(bool shouldBeTrue, const std::source_location location = std::source_location::current()) noexcept
{
	if (likely(shouldBeTrue))
	{
		return;
	}

	FlushLogs();
	PrintArgs("[Assert] ", location.file_name(), ":", location.line(),
			  " failed. A message-less assert has nothing else to say, so the call site is the whole report.");

	debugBreak();
	std::abort();
}

template <typename... Types>
void Assert(bool shouldBeTrue, Types&&... args) noexcept
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
inline void Assert(bool) noexcept
{
}

template <typename... Types>
void Assert(bool, Types&&...) noexcept
{
}
} // namespace hbe
#endif // __DEBUG__

namespace hbe
{
inline void FatalAssert(bool shouldBeTrue, const std::source_location location = std::source_location::current())
{
	if (likely(shouldBeTrue))
	{
		return;
	}

	FlushLogs();
	PrintArgs("[FatalAssert] ", location.file_name(), ":", location.line(),
			  " failed. A message-less FatalAssert has no other way to say what it caught.");
	debugBreak();
	std::abort();
}

template <typename... Types>
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
