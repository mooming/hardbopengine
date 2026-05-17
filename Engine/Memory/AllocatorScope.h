// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "AllocatorID.h"

namespace hbe
{

	/// @brief RAII wrapper for temporarily changing the scoped allocator.
	/// @details Saves the current allocator ID on construction and restores it on destruction.
	/// Used to temporarily redirect allocations to a specific allocator within a scope.
	class AllocatorScope final
	{
	public:
		AllocatorScope(const AllocatorScope&) = delete;
		AllocatorScope(AllocatorScope&&) = delete;
		AllocatorScope& operator=(const AllocatorScope&) noexcept = delete;
		AllocatorScope& operator=(AllocatorScope&&) noexcept = delete;

	public:
		AllocatorScope() noexcept;
		AllocatorScope(TAllocatorID id) noexcept;

		template<typename T>
		AllocatorScope(const T& allocator) noexcept : AllocatorScope(allocator.GetID())
		{}

		~AllocatorScope() noexcept;

	private:
		TAllocatorID previous;
		TAllocatorID current;
	};

} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{

	class AllocatorScopeTest : public TestCollection
	{
	public:
		AllocatorScopeTest() : TestCollection("AllocatorScopeTest") {}

	protected:
		void Prepare() override;
	};

} // namespace hbe

#endif //__UNIT_TEST__
