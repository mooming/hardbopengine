// Created by mooming.go@gmail.com, 2022

#pragma once

namespace HE
{

using TAllocatorID = int;
static constexpr TAllocatorID MaxNumAllocators = 128;
static constexpr TAllocatorID InvalidAllocatorID = -1;

inline bool IsValid(TAllocatorID id)
{
    return id >= 0 && id < MaxNumAllocators;
}

} // namespace HE
