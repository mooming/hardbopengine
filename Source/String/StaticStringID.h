// Created by mooming.go@gmail.com, 2022

#pragma once

#include <cstdint>
#include <ostream>

namespace HE
{
    struct StaticStringID final
    {
        static constexpr uint8_t* Default = nullptr;
        const uint8_t* ptr;

        inline StaticStringID()
            : ptr(Default)
        {
        }

        inline bool operator==(const StaticStringID& rhs) const
        {
            return ptr == rhs.ptr;
        }

        inline bool operator<(const StaticStringID& rhs) const
        {
            return ptr < rhs.ptr;
        }
    };

} // namespace HE

namespace std
{
    template <>
    struct hash<HE::StaticStringID> final
    {
        std::size_t operator()(const HE::StaticStringID& obj) const
        {
            return reinterpret_cast<std::size_t>(obj.ptr);
        }
    };
} // namespace std
