// Created by mooming.go@gmail.com, 2017

#pragma once

#include <array>
#include <ostream>

namespace HE
{

    enum class ComponentState : int
    {
        NONE,
        BORN,
        ALIVE,
        SLEEP,
        DEAD
    };

    inline std::ostream& operator<<(
        std::ostream& os, const ComponentState& state)
    {
        static const char* names[] = {"NONE", "BORN", "ALIVE", "SLEEP", "DEAD"};

        os << names[static_cast<int>(state)];
        return os;
    }
} // namespace HE
