// Created by mooming.go@gmail.com, 2017

#pragma once

#include <cstdint>


// Definitions for Pre-compilation

// Engine
#define ENGINE_LOG_ENABLED

// Allocator
#define __MEMORY_VERIFICATION__
#define __MEMORY_STATISTICS__
#define __MEMORY_LOGGING__
//#define __MEMORY_INVESTIGATION__
//#define __MEMORY_INVESTIGATION_LOGGING__
#define __MEMORY_DANGLING_POINTER_CHECK__
//#define __MEMORY_BUFFER_UNDERRUN_CHECK__
//#define __USE_SYSTEM_MALLOC__

// Performance
#define __PERFORMANCE_COUNTER__

// Log
#define LOG_ENABLED

// Debug Control
#if !defined(NDEBUG) || defined(_DEBUG) || defined(DEBUG)
#undef __DEBUG__
#define __DEBUG__
#endif // _DEBUG

// Mathematics
//#define __LEFT_HANDED__
#define __RIGHT_HANDED__


// Test
//#define __MEMORY_INVESTIGATOR_TEST__

// Default Setting Values
namespace HE
{
namespace Config
{
// Engine
static constexpr uint8_t EngineLogLevel = 1;
static constexpr uint8_t EngineLogLevelPrint = 1;

// Memory
static constexpr uint8_t MemLogLevel = 4;

// String
static constexpr int MaxPathLength = 512;
static constexpr int StaticStringBufferSize = 8 * 1024 * 1024;
static constexpr int StaticStringNumHashBuckets = 256;

// Log
static constexpr int LogLineSize = 1000;
static constexpr int LogBufferSize = 1024;
static constexpr int LogMemoryBlockSize = 1024 + 256;
static constexpr int LogNumMemoryBlocks = 1024 * 16;
static constexpr int LogForceFlushThreshold = 1024 * 8;

// TaskSystem
static constexpr int MaxConcurrentTasks = 32;
} // Config
} // HE
