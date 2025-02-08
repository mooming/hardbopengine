// Created by mooming.go@gmail.com

#include "MultiPoolAllocatorConfig.h"

namespace HE
{

    MultiPoolAllocatorConfig::MultiPoolAllocatorConfig(
        StaticStringID id, TPoolConfigs&& inConfigs)
        : uniqueName(id),
          configs(std::move(inConfigs))
    {
    }

    bool MultiPoolAllocatorConfig::operator<(
        const MultiPoolAllocatorConfig& rhs) const
    {
        return uniqueName < rhs.uniqueName;
    }

} // namespace HE
