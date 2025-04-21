// Created by mooming.go@gmail.com

#pragma once

#include "MultiPoolAllocatorConfig.h"
#include "String/StaticString.h"

namespace HE
{

class Buffer;

class MultiPoolConfigCache final
{
public:
    using TVersion = uint32_t;

    template <typename T>
    using TVector = std::vector<T>;
    using TMultiPoolConfigs = TVector<MultiPoolAllocatorConfig>;

private:
    static constexpr TVersion version = 0;
    TMultiPoolConfigs data;

public:
    StaticString GetClassName() const;

    size_t Serialize(Buffer& outBuffer);
    bool Deserialize(const Buffer& buffer);

    inline auto GetVersion() const { return version; }

    inline auto& GetData() { return data; }
    inline auto& GetData() const { return data; }

private:
    void Normalize();
};

} // namespace HE
