// Created by mooming.go@gmail.com

#pragma once

#include "PoolConfig.h"
#include "String/StaticStringID.h"
#include <vector>


namespace HE
{

struct MultiPoolAllocatorConfig final
{
    template <typename T>
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

} // namespace HE
