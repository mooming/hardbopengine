// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <vector>
#include "PoolConfig.h"
#include "String/StaticStringID.h"

namespace hbe
{
	/// @brief Configuration for a multi-pool allocator.
	/// @details Stores name and pool configurations for a multi-pool allocator.
	class MultiPoolAllocatorConfig final
	{
	public:
		template<typename T>
		using TVector = std::vector<T>;
		using TPoolConfigs = TVector<PoolConfig>;

		StaticStringID uniqueName;
		TPoolConfigs configs;

		MultiPoolAllocatorConfig() noexcept = default;
		MultiPoolAllocatorConfig(StaticStringID id, TPoolConfigs&& configs) noexcept;

		bool operator<(const MultiPoolAllocatorConfig& rhs) const noexcept;
	};

} // namespace hbe
