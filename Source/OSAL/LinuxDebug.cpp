// Created by mooming.go@gmail.com

#include "OSDebug.h"

#ifdef PLATFORM_LINUX

using namespace hbe;

StaticString OS::GetBackTrace(uint16_t, uint16_t)
{
    return StaticString("Not Implemented");
}

#endif // PLATFORM_LINUX
