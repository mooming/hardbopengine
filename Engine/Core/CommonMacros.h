// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

/// API reference: docs/Core/index.html#commonmacros
#define ReturnIf(...)                                                                                                  \
	if (static_cast<bool>(__VA_ARGS__))                                                                                \
	return
#define ReturnValueIf(returnValue, ...)                                                                                \
	if (static_cast<bool>(__VA_ARGS__))                                                                                \
	return returnValue
#define BreakIf(...)                                                                                                   \
	if (static_cast<bool>(__VA_ARGS__))                                                                                \
	break
#define ContinueIf(...)                                                                                                \
	if (static_cast<bool>(__VA_ARGS__))                                                                                \
	continue
#define ONCE() while (false)
