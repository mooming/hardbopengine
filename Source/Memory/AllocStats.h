// Created by mooming.go@gmail.com, 2022

#pragma once

#include "String/StaticStringID.h"
#include <cstddef>


namespace HE
{

struct AllocStats final
{
    static constexpr size_t NameBufferSize = 63;

    char name[NameBufferSize] = "";
    bool isInline = false;

    size_t capacity = 0;
    size_t usage = 0;
    size_t maxUsage = 0;

    size_t totalRequested = 0;
    size_t maxRequested = 0;
    size_t totalFallback = 0;
    size_t maxFallback = 0;

    size_t allocCount = 0;
    size_t deallocCount = 0;
    size_t fallbackCount = 0;

    AllocStats();
    ~AllocStats() = default;
    
    void OnRegister(const char* name, bool inIsInnline, size_t inCapacity);
    void Reset();
    void Report();

    void Print();
};

} // HE
