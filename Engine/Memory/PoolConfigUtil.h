// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once
#include <vector>
#include "PoolConfig.h"

namespace hbe
{
	/// @brief Utility functions for pool configuration management.
	namespace PoolConfigUtil
	{

		using TPoolConfigs = std::vector<PoolConfig>;

		void Normalize(TPoolConfigs& configs);
		void MergeMax(TPoolConfigs& dst, TPoolConfigs& src);

	} // namespace PoolConfigUtil

} // namespace hbe
