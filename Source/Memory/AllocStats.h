// Created by mooming.go@gmail.com

#pragma once

#include "String/StaticString.h"
#include <cstddef>


namespace HE
{

struct AllocStats final
{
    StaticString name;
    bool isInline;

    size_t capacity;
    size_t usage;
    size_t maxUsage;

    size_t totalRequested;
    size_t maxRequested;
    size_t totalFallback;
    size_t maxFallback;

    size_t allocCount;
    size_t deallocCount;
    size_t fallbackCount;

    AllocStats();
    ~AllocStats() = default;

    void OnRegister(const char* name, bool inIsInnline, size_t inCapacity);
    void Reset();
    void Report();

    void Print();
};

} // namespace HE
