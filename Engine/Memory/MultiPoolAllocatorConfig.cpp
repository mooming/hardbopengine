// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "MultiPoolAllocatorConfig.h"


namespace hbe
{

	MultiPoolAllocatorConfig::MultiPoolAllocatorConfig(StaticStringID id, TPoolConfigs&& inConfigs) noexcept :
		uniqueName(id), configs(std::move(inConfigs))
	{}

	bool MultiPoolAllocatorConfig::operator<(const MultiPoolAllocatorConfig& rhs) const noexcept
	{
		return uniqueName < rhs.uniqueName;
	}

} // namespace hbe
