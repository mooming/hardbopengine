// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "PipelineTypes.h"


namespace hbe::pipeline
{
class PipelineDefinition
{
	using TJob = void (*)(TSize index);
public:
	[[nodiscard]] uint32_t GetMaxConcurrency() const;
	[[nodiscard]] uint32_t GetStageCount() const;

	uint32_t AddStage();
	void AddJob(TStageID stageIndex, TJob job, uint8_t numConcurrency = 1);
};
} // namespace hbe
