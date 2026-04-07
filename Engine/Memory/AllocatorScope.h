// Created by mooming.go@gmail.com

#pragma once

#include "AllocatorID.h"

namespace hbe
{

	/// @brief RAII wrapper for temporarily changing the scoped allocator.
	/// @details Saves the current allocator ID on construction and restores it on destruction.
	/// Used to temporarily redirect allocations to a specific allocator within a scope.
	class AllocatorScope final
	{
	private:
		TAllocatorID previous;
		TAllocatorID current;

	public:
		AllocatorScope(const AllocatorScope&) = delete;
		AllocatorScope(AllocatorScope&&) = delete;
		AllocatorScope& operator=(const AllocatorScope&) = delete;
		AllocatorScope& operator=(AllocatorScope&&) = delete;

	public:
		AllocatorScope();
		AllocatorScope(TAllocatorID id);

		template<typename T>
		AllocatorScope(const T& allocator) : AllocatorScope(allocator.GetID())
		{}

		~AllocatorScope();
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
		virtual void Prepare() override;
	};

} // namespace hbe

#endif //__UNIT_TEST__
