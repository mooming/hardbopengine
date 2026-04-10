// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstddef>
#include <cstdint>
#include "Config/BuildConfig.h"
#include "Config/EngineConfig.h"

#define ALIGN alignas(hbe::Config::DefaultAlign)

namespace OS
{

	/// @brief Provides low-level memory management operations for platform-independent memory allocation,
	/// alignment, and protection.
	bool IsValidAllocation(void* ptr);

	template<typename T>
	bool CheckAligned(T* ptr, uint32_t alignBytes = hbe::Config::DefaultAlign)
	{
		const size_t address = reinterpret_cast<size_t>(ptr);
		return (address % alignBytes) == 0;
	}

	constexpr size_t GetAligned(size_t size, uint32_t alignBytes = hbe::Config::DefaultAlign)
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

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{

	/// @brief Test collection for OS memory operations.
	class OSMemoryTest : public TestCollection
	{
	public:
		inline OSMemoryTest() : TestCollection("OSMemoryTest") {}

	protected:
		virtual void Prepare() override;
	};

} // namespace hbe
#endif //__UNIT_TEST__
