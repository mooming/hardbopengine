// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "OSDebug.h"

#ifdef PLATFORM_LINUX

using namespace hbe;

StaticString OS::GetBackTrace(uint16_t, uint16_t) { return StaticString("Not Implemented"); }

#endif // PLATFORM_LINUX
