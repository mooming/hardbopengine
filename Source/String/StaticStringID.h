// Created by mooming.go@gmail.com, 2022

#pragma once

#include <cstdint>


namespace HE
{
struct StaticStringID final
{
    using TIndex = uint16_t;
    
    union
    {
        uint64_t value = 0;
        
        struct
        {
            TIndex tableID;
            TIndex index;
        };
    };
};

} // HE

