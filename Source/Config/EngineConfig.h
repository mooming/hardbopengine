// Created by mooming.go@gmail.com, 2017

#pragma once

// Definitions for Pre-compilation
//#define __TEST__
//#define __UNIT_TEST__
#define __VERIFY_MEMORY__
//#define __VERIFY_MEMORY_HEAVY__
#define __USE_SYSTEM_MALLOC__

// Debug Control
#if !defined(NDEBUG) || defined(_DEBUG) || defined(DEBUG)
#undef __DEBUG__
#define __DEBUG__
#endif // _DEBUG

// Mathematics
//#define __LEFT_HANDED__
#define __RIGHT_HANDED__

// Default Setting Values
namespace HE
{
    namespace Config
    {
        static constexpr int StaticStringChunkSize = 4 * 1024 * 1024;
    }
} // HE
