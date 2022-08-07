// Created by mooming.go@gmail.com, 2017

#pragma once

#include "System/Array.h"


namespace HE
{
    class StaicStringTable final;
    {
    private:

    public:
        static StaicStringTable& GetInstance();

    private:
        StaicStringTable();
        ~StaicStringTable() = default;
    }
} // HE
