// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstdint>
#include <cstdio>

#include "BuildConfig.h"


namespace hbe
{
namespace Config
{
/// API reference: docs/Config/EngineConfig/index.html
static constexpr uint8_t EngineLogLevel = MEMORY_LOGGING_ENABLED ? 0 : 1;

static constexpr uint8_t EngineLogLevelPrint = MEMORY_LOGGING_ENABLED ? 1 : 2;
static_assert(EngineLogLevel <= EngineLogLevelPrint,
			  "EngineLogLevelPrint should be greater than or equal to EngineLogLevel");

static constexpr size_t MemCapacity = (5ULL * 1024 * 1024 * 1024);

static constexpr uint8_t MemLogLevel = 1;

static constexpr size_t DefaultAlign = 16;

static constexpr int MaxPathLength = 512;
static constexpr int StaticStringBufferSize = 8 * 1024 * 1024;
static constexpr int StaticStringNumHashBuckets = 256;

static constexpr int LogLineLength = 1024;
static constexpr int LogOutputBuffer = LogLineLength * 128;
static constexpr int LogMemoryBlockSize = LogLineLength * 256;
static constexpr int LogNumMemoryBlocks = 1024 * 12;
static constexpr int LogForceFlushThreshold = 1024 * 8;

static constexpr int MaxConcurrentTasks = 32;

static constexpr float DebugTimeOutMultiplier = 2.0f;

[[nodiscard]] size_t GetMaxSystemMemoryTarget() noexcept;
} // namespace Config
} // namespace hbe
