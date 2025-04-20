// Created by mooming.go@gmail.com 2022

#pragma once

#include "Config/BuildConfig.h"
#include "Config/EngineConfig.h"
#include <cstddef>
#include <cstdint>

#define ALIGN alignas(HE::Config::DefaultAlign)

namespace OS
{

    template <typename T>
    bool CheckAligned(T* ptr, uint32_t alignBytes = HE::Config::DefaultAlign)
    {
        const size_t address = reinterpret_cast<size_t>(ptr);
        return (address % alignBytes) == 0;
    }

    constexpr size_t GetAligned(
        size_t size, uint32_t alignBytes = HE::Config::DefaultAlign)
    {
        const auto multiplier = (size + alignBytes - 1) / alignBytes;
        return multiplier * alignBytes;
    }

    size_t GetAllocSize(void* ptr);
    size_t GetPageSize();
    void* VirtualAlloc(size_t size);
    void VirtualFree(void* address, std::size_t n);
    void ProtectMemory(void* address, std::size_t n);

} // namespace OS
