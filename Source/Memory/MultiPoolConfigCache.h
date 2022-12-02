// Created by mooming.go@gmail.com, 2022

#pragma once

#include "MemoryManager.h"
#include "PoolConfig.h"
#include "String/StaticString.h"
#include "String/StaticStringID.h"
#include <ostream>


namespace HE
{

class Buffer;

class MultiPoolConfigCache final
{
public:
    using TVersion = uint32_t;
    using TKey = StaticStringID;
    using TValue = PoolConfig;
    using TPoolConfigs = MemoryManager::TPoolConfigs;
    using TCacheContainer = MemoryManager::TMultiPoolConfigCache;

    static constexpr TVersion version = 0;

private:
    TCacheContainer data;

public:
    MultiPoolConfigCache() = default;
    ~MultiPoolConfigCache() = default;

    StaticString GetClassName() const;

    void Swap(TCacheContainer& inOutData);

    size_t Serialize(Buffer& outBuffer);
    bool Deserialize(const Buffer& buffer);

    inline auto GetVersion() const { return version; }

    inline auto& GetData() { return data; }
    inline auto& GetData() const { return data; }
};

} // HE
