// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstddef>
#include <cstdint>

#include "Config/BuildConfig.h"
#include "Config/EngineConfig.h"

#define HE_ALIGN alignas(hbe::Config::DefaultAlign)

namespace OS
{

/// @brief Provides low-level memory management operations for platform-independent memory allocation,
/// alignment, and protection.
[[nodiscard]] bool IsValidAllocation(void* ptr) noexcept;

template<typename T>
[[nodiscard]] bool CheckAligned(T* ptr, uint32_t alignBytes = hbe::Config::DefaultAlign) noexcept
{
	const size_t address = reinterpret_cast<size_t>(ptr);
	return (address % alignBytes) == 0;
}

[[nodiscard]] constexpr size_t GetAligned(size_t size, uint32_t alignBytes = hbe::Config::DefaultAlign) noexcept
{
	const auto multiplier = (size + alignBytes - 1) / alignBytes;
	return multiplier * alignBytes;
}

[[nodiscard]] size_t GetAllocSize(void* ptr) noexcept;
[[nodiscard]] size_t GetPageSize() noexcept;
void* VirtualAlloc(size_t size);
void VirtualFree(void* address, std::size_t n) noexcept;
void ProtectMemory(void* address, std::size_t n) noexcept;

} // namespace OS

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{

/// @brief Test collection for OS memory operations.
class OSMemoryTest final : public TestCollection
{
public:
	OSMemoryTest() : TestCollection("OSMemoryTest") {}

protected:
	void Prepare() override;
};

} // namespace hbe
#endif //__UNIT_TEST__
