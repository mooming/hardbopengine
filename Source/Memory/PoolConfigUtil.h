// Created by mooming.go@gmail.com

#pragma once
#include "PoolConfig.h"
#include <vector>

namespace HE
{

    namespace PoolConfigUtil
    {

        using TPoolConfigs = std::vector<PoolConfig>;

        void Normalize(TPoolConfigs& configs);
        void MergeMax(TPoolConfigs& dst, TPoolConfigs& src);

    } // namespace PoolConfigUtil

} // namespace HE
