// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "MultiPoolAllocatorConfig.h"

namespace hbe
{

	MultiPoolAllocatorConfig::MultiPoolAllocatorConfig(StaticStringID id, TPoolConfigs&& inConfigs) :
		uniqueName(id), configs(std::move(inConfigs))
	{}

	bool MultiPoolAllocatorConfig::operator<(const MultiPoolAllocatorConfig& rhs) const
	{
		return uniqueName < rhs.uniqueName;
	}

} // namespace hbe
