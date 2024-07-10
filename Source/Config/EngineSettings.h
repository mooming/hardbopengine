// Created by mooming.go@gmail.com

#pragma once
#include <cstddef>
#include <cstdint>


// Default Setting Values
namespace HE
{

namespace Config
{

// Engine
static constexpr uint8_t EngineLogLevel =
    2; // 0: Verbose, 1: Info, 2: Significant, 3: Warning, 4: Error, 5: FatalError
static constexpr uint8_t EngineLogLevelPrint = 0;

// Memory
static constexpr size_t MemCapacity = (5ULL * 1024 * 1024 * 1024); // 5 GB
static constexpr uint8_t MemLogLevel =
    2; // 0: Verbose, 1: Info, 2: Significant, 3: Warning, 4: Error, 5: FatalError
static constexpr size_t DefaultAlign = 16; // 16 Bytes Alignment

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
