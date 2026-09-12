// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstddef>
#include "String/StaticString.h"

namespace hbe
{

	class AllocStats final
	{
	public:
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

		void OnRegister(const char* name, bool isInline, size_t inCapacity) noexcept;
		void Reset() noexcept;
		void Report() noexcept;

		void Print() noexcept;
	};

} // namespace hbe
