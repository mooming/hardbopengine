// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#define returnIf(...) if (static_cast<bool>(__VA_ARGS__)) return
#define returnValueIf(returnValue, ...) if (static_cast<bool>(__VA_ARGS__)) return returnValue
#define breakIf(...) if (static_cast<bool>(__VA_ARGS__)) break
#define continueIf(...) if (static_cast<bool>(__VA_ARGS__)) continue
#define ONCE() while(false) // do {} ONCE();
