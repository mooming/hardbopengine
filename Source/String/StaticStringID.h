// Created by mooming.go@gmail.com, 2022

#pragma once

#include <cstdint>
#include <ostream>


namespace HE
{
struct StaticStringID final
{
    static constexpr uint64_t Default = 0;
    using TIndex = uint32_t;
    
    union
    {
        uint64_t value;
        
        struct
        {
            TIndex tableID;
            TIndex index;
        };
    };

    inline StaticStringID()
        : value(Default)
    {
    }

    inline bool operator == (const StaticStringID& rhs) const
    {
        return value == rhs.value;
    }

    inline bool operator < (const StaticStringID& rhs) const
    {
        return value < rhs.value;
    }
};

} // HE

namespace std
{
template<>
struct hash<HE::StaticStringID> final
{
    std::size_t operator() (const HE::StaticStringID& obj) const
    {
        return static_cast<std::size_t>(obj.value);
    }
};
} // std


