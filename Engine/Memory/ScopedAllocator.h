// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "AllocatorScope.h"

namespace hbe
{
	/// @brief Wrapper that ties an allocator to an allocation scope.
	/// @details RAII wrapper that sets the current allocator on construction
	/// and restores the previous scope on destruction.
	template<typename TAlloc>
	class ScopedAllocator final
	{
	private:
		TAlloc allocator;
		AllocatorScope scope;

	public:
		ScopedAllocator(const ScopedAllocator&) = delete;
		ScopedAllocator(ScopedAllocator&&) = delete;
		ScopedAllocator& operator=(const ScopedAllocator&) = delete;
		ScopedAllocator& operator=(ScopedAllocator&&) = delete;

	public:
		template<typename... Types>
		ScopedAllocator(Types&&... args) : allocator(std::forward<Types>(args)...), scope(allocator)
		{}

		~ScopedAllocator() = default;

		auto& GetAllocator() { return allocator; }
		auto& GetAllocator() const { return allocator; }
	};

} // namespace hbe
