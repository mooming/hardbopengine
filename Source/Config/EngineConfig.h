// Created by mooming.go@gmail.com

#pragma once

#include "BuildConfig.h"
#include <cstdint>
#include <cstdio>

// Static Engine Default Settings

namespace HE
{

namespace Config
{

// Engine

// 0: Verbose, 1: Info, 2: Significant, 3: Warning, 4: Error, 5:
// FatalError

// Engine Log Level to Log
static constexpr uint8_t EngineLogLevel = __MEMORY_LOGGING__ ? 0 : 1;

// Engine Log Level to Print on STD OUT
static constexpr uint8_t EngineLogLevelPrint = __MEMORY_LOGGING__ ? 1 : 2;

static_assert(EngineLogLevel <= EngineLogLevelPrint,
    "EngineLogLevelPrint should be greater than or equal to EngineLogLevel");

// Memory
static constexpr size_t MemCapacity = (5ULL * 1024 * 1024 * 1024); // 5 GB

// Default Memory Engine Log Level
// 0: Verbose, 1: Info, 2: Significant, 3: Warning, 4: Error, 5:FatalError
static constexpr uint8_t MemLogLevel = 1;

// 16 Bytes Alignment
static constexpr size_t DefaultAlign = 16;

// String
static constexpr int MaxPathLength = 512;
static constexpr int StaticStringBufferSize = 8 * 1024 * 1024;
static constexpr int StaticStringNumHashBuckets = 256;

// Log
static constexpr int LogLineLength = 1024;
static constexpr int LogOutputBuffer = LogLineLength * 128;
static constexpr int LogMemoryBlockSize = LogLineLength * 256;
static constexpr int LogNumMemoryBlocks = 1024 * 12;
static constexpr int LogForceFlushThreshold = 1024 * 8;

// TaskSystem
static constexpr int MaxConcurrentTasks = 32;

// Profile
static constexpr float DebugTimeOutMultiplier = 2.0f;

size_t GetMaxSystemMemoryTarget();

} // namespace Config

} // namespace HE
