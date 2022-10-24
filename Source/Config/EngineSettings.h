// Created by mooming.go@gmail.com, 2022

#pragma once
#include <cstddef>
#include <cstdint>


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
static constexpr size_t DefaultAlign = 16;

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

size_t GetMaxSystemMemoryTarget();
} // Config

} // HE
