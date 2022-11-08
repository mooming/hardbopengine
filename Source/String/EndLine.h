// Created by mooming.go@gmail.com, 2022

#pragma once

namespace HE
{

struct EndLine final
{
    operator const char* () const
    {
        return "\n";
    }
};

static constexpr EndLine hendl;

} // HE
