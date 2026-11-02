// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstdint>

#include "PipelineTypes.h"
#include "String/StaticString.h"
#include "TaskStreamIndex.h"


namespace hbe::pipeline
{
class PipelineStageConfig final
{
public:
	static constexpr TSize AutoCapacity = 0;

	StaticString name = "";
	TStreamIndex stream = 0;
	uint8_t priority = 0;
	TSize batchSize = 0;
	TSize capacity = AutoCapacity;
};
} // namespace hbe::pipeline
