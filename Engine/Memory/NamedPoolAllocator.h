// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstddef>

#include "MultiPoolAllocator.h"

namespace hbe
{

/// @brief Allocator that draws from a pool someone else named, rather than from the ambient scope.
/// @details Every other container in the engine takes `DefaultAllocator` by default, which captures
///          `MemoryManager::GetCurrentAllocatorID()` at the moment it is constructed - so a container that must
///          live in one specific pool gets whatever pool happened to be in scope where it was built instead. This
///          adapter states the pool as a constructor argument, so a container can be given an owner: the memory
///          of a thing is decided by whoever asked for the thing, not by call order.
/// @details It satisfies the allocator shape the engine's containers use - `allocate(count)` and
///          `deallocate(ptr, count)` in elements - by scaling to bytes against the pool, which allocates in
///          bytes. `value_type` is declared as well so `hbe::New` and `hbe::Delete` accept it.
/// @note Alignment is limited by the pool, not by this adapter. A pool serves blocks out of a buffer whose
///       stride is aligned to `Config::DefaultAlign`, which is 16 bytes today, so an element needing more than
///       that has to be given a pool able to promise it. The static_assert below states the bound at the place a
///       violation would otherwise be a silent misalignment.
/// @note A pool is not thread-safe and this adapter does not make it one. It hands out the pool it was given, so
///       every rule about which thread may allocate from that pool applies to every container drawing from here.
template <typename T>
class NamedPoolAllocator final
{
public:
	using value_type = T;

	/// @brief Largest alignment a pool can promise, which is `Config::DefaultAlign`.
	static constexpr std::size_t MaxSupportedAlignment = 16;

	static_assert(alignof(T) <= MaxSupportedAlignment,
				  "A pool serves blocks aligned to Config::DefaultAlign; this element needs more than that.");

public:
	/// @param pool The pool to allocate from, for the whole life of this allocator and of everything allocating
	///        through it. It must outlive every container holding a copy of this allocator.
	explicit NamedPoolAllocator(MultiPoolAllocator& pool) noexcept
		: pool(&pool)
	{
	}

	[[nodiscard]] T* allocate(std::size_t n) noexcept
	{
		return static_cast<T*>(pool->Allocate(n * sizeof(T)));
	}

	void deallocate(T* ptr, std::size_t n) noexcept
	{
		pool->Deallocate(ptr, n * sizeof(T));
	}

	[[nodiscard]] MultiPoolAllocator& GetPool() const noexcept
	{
		return *pool;
	}

private:
	MultiPoolAllocator* pool = nullptr;
};

} // namespace hbe
