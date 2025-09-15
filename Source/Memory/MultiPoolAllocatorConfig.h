// Created by mooming.go@gmail.com

#pragma once

#include <vector>
#include "PoolConfig.h"
#include "String/StaticStringID.h"

namespace hbe
{

	struct MultiPoolAllocatorConfig final
	{
		template<typename T>
		using TVector = std::vector<T>;
		using TPoolConfigs = TVector<PoolConfig>;

	public:
		StaticStringID uniqueName;
		TPoolConfigs configs;

	public:
		MultiPoolAllocatorConfig() = default;
		MultiPoolAllocatorConfig(StaticStringID id, TPoolConfigs&& configs);

		bool operator<(const MultiPoolAllocatorConfig& rhs) const;
	};

} // namespace hbe
