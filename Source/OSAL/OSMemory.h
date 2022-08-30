// Created by mooming.go@gmail.com 2022

#pragma once

#include <cstddef>

#define ALIGN16 alignas(16)


namespace OS
{

template <typename T>
bool CheckAligned(T *ptr, unsigned int alignBytes = 16)
{
    const size_t address = reinterpret_cast<size_t>(ptr);
    return (address % alignBytes) == 0;
}

size_t GetAllocSize(void* ptr);
size_t GetPageSize();
void ProtectMemory(void* address, std::size_t n);

} // OSAL
