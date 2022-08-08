// Created by mooming.go@gmail.com, 2017

#pragma once

// Definitions for Pre-compilation
//#define __TEST__
//#define __UNIT_TEST__

// Allocator
//#define __MEMORY_VERIFICATION__
//#define __MEMORY_STATISTICS__
//#define __MEMORY_LOGGING__
//#define __MEMORY_INVESTIGATION__
//#define __MEMORY_OVERFLOW_CHECK__ 1 // 0: under-run, 1: over-run
//#define __USE_SYSTEM_MALLOC__

// Performance
#define __PERFORMANCE_COUNTER__

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
