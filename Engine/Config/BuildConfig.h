// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

// =============================================================================
// Build Configuration
// =============================================================================
// This file contains compile-time configuration settings for the engine.
// Most settings can be left at their defaults, but can be modified as needed.

// =============================================================================
// Platform Detection
// =============================================================================
// Automatically detected from compiler macros:
// Only ONE of these should be defined (as 1):
//   - PLATFORM_LINUX  : Running on Linux
//   - PLATFORM_OSX    : Running on macOS
//   - PLATFORM_WINDOWS: Running on Windows

#ifdef __linux__
    #define PLATFORM_LINUX 1
#elif defined __APPLE__
    #define PLATFORM_OSX 1
#elif defined _WIN32
    #define PLATFORM_WINDOWS 1
#endif

// Ensure exactly one platform is defined
#if !defined(PLATFORM_LINUX) && !defined(PLATFORM_OSX) && !defined(PLATFORM_WINDOWS)
    static_assert(false, "No platform defined. Please define PLATFORM_LINUX, PLATFORM_OSX, or PLATFORM_WINDOWS.");
#endif

// =============================================================================
// Engine Configuration
// =============================================================================
#define MAX_NUM_TASK_STREAMS 64  // Maximum number of task streams in TaskSystem

// =============================================================================
// Debug Control
// =============================================================================
// Enables debug mode when NDEBUG, _DEBUG, or DEBUG is not defined
// Set to 0 to disable debug assertions in release builds

// =============================================================================
// System Requirements
// =============================================================================
#define ENGINE_MIN_HARDWARE_THREADS 4  // Minimum recommended CPU cores

// =============================================================================
// Engine Core
// =============================================================================
#define ENGINE_LOG_ENABLED 1       // Enable/disable logging system
#define ENGINE_PARAM_DESC_ENABLED 1 // Enable parameter descriptions in ConfigParam

// =============================================================================
// Memory System
// =============================================================================
// Memory debugging features (set to 1 to enable, 0 for performance)
#define __MEMORY_VERIFICATION__ 0          // Verify memory integrity
#define __MEMORY_LOGGING__ 0               // Log memory operations
#define __MEMORY_INVESTIGATION__ 0          // Detailed memory investigation
#define __MEMORY_DANGLING_POINTER_CHECK__ 0 // Detect dangling pointers
#define __MEMORY_BUFFER_UNDERRUN_CHECK__ 0  // Detect buffer underruns
#define __FORCE_USE_SYSTEM_MALLOC__ 0        // Force use of system malloc instead of custom allocators
#define MULTIPOOL_ALLOC_LOG ".multiPoolConfig.dat"  // MultiPool config cache file

// =============================================================================
// Logging System
// =============================================================================
#define LOG_ENABLED 1                  // Master switch for logging
#define LOG_BREAK_IF_WARNING 0         // Break on warnings (debug only)
#define LOG_BREAK_IF_ERROR 0           // Break on errors (debug only)
#define LOG_FORCE_PRINT_IMMEDIATELY 0  // Bypass async logging, print immediately

// =============================================================================
// Profiling
// =============================================================================
// Set to 1 to enable performance profiling (adds overhead)
#define PROFILE_ENABLED 0

// =============================================================================
// Mathematics
// =============================================================================
// Coordinate system handedness (uncomment to change)
// #define __LEFT_HANDED__
#define __RIGHT_HANDED__  // Default

// =============================================================================
// Testing
// =============================================================================
#define __MEMORY_INVESTIGATOR_TEST__ 0  // Enable memory investigation tests

// =============================================================================
// Third-Party Integration
// =============================================================================
// Vulkan SDK detection (auto-detected, do not modify)
// Set to 1 if vulkan.h is found, 0 otherwise
// Will produce compile warning if not found but code will still compile
#if __has_include("vulkan/vulkan.h")
    #define VULKAN_SDK 1
#else
    #define VULKAN_SDK 0
    #warning "Vulkan SDK not found. Vulkan support will be disabled."
#endif