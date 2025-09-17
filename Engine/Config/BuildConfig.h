// Created by mooming.go@gmail.com, 2022

#pragma once

// Definitions for Pre-compilation

// Platform
#ifdef __linux__
#define PLATFORM_LINUX
#elif defined __APPLE__
#define PLATFORM_OSX
#elif defined _WIN32
#define PLATFORM_WINDOWS
#endif

// Debug Control
#if !defined(NDEBUG) || defined(_DEBUG) || defined(DEBUG)
#undef __DEBUG__
#define __DEBUG__
#endif // _DEBUG

// Engine
#define ENGINE_LOG_ENABLED 1
#define ENGINE_PARAM_DESC_ENABLED 1

// Allocator
#define MULTIPOOL_ALLOC_LOG ".multiPoolConfig.dat"
#define __MEMORY_VERIFICATION__ 0
#define __MEMORY_LOGGING__ 0
#define __MEMORY_INVESTIGATION__ 0
#define __MEMORY_DANGLING_POINTER_CHECK__ 0
#define __MEMORY_BUFFER_UNDERRUN_CHECK__ 0
#define __FORCE_USE_SYSTEM_MALLOC__ 0

// Log
#define LOG_ENABLED 1
#define LOG_BREAK_IF_WARNING 0
#define LOG_BREAK_IF_ERROR 1
#define LOG_FORCE_PRINT_IMMEDIATELY 0

// Profile
#define PROFILE_ENABLED 0

// Mathematics
// #define __LEFT_HANDED__
#define __RIGHT_HANDED__

// Test
#define __MEMORY_INVESTIGATOR_TEST__ 0
