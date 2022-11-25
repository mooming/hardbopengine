// Created by mooming.go@gmail.com 2022

#include "OSDebug.h"


#ifdef PLATFORM_LINUX

using namespace HE;

StaticString OS::GetBackTrace(uint16_t, uint16_t)
{
    return StaticString("Not Implemented");
}

#endif // PLATFORM_LINUX
