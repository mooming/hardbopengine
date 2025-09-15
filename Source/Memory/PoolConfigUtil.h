// Created by mooming.go@gmail.com

#pragma once
#include <vector>
#include "PoolConfig.h"

namespace hbe
{

	namespace PoolConfigUtil
	{

		using TPoolConfigs = std::vector<PoolConfig>;

		void Normalize(TPoolConfigs& configs);
		void MergeMax(TPoolConfigs& dst, TPoolConfigs& src);

	} // namespace PoolConfigUtil

} // namespace hbe
