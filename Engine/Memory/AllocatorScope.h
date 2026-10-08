// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "AllocatorID.h"


namespace hbe
{
/// API reference: docs/Memory/AllocatorScope/index.html
class AllocatorScope final
{
private:
	TAllocatorID previous;
	TAllocatorID current;

public:
	AllocatorScope(const AllocatorScope&) = delete;
	AllocatorScope(AllocatorScope&&) = delete;
	AllocatorScope& operator=(const AllocatorScope&) noexcept = delete;
	AllocatorScope& operator=(AllocatorScope&&) noexcept = delete;

public:
	AllocatorScope() noexcept;
	AllocatorScope(TAllocatorID id) noexcept;

	template <typename T>
	AllocatorScope(const T& allocator) noexcept
		: AllocatorScope(allocator.GetID())
	{
	}

	~AllocatorScope() noexcept;
};
} // namespace hbe

#ifdef TEST_ENABLED
#include "Test/TestCollection.h"

namespace hbe
{
class AllocatorScopeTest : public TestCollection
{
public:
	AllocatorScopeTest()
		: TestCollection("AllocatorScopeTest")
	{
	}

protected:
	void Prepare() override;
};
} // namespace hbe

#endif //TEST_ENABLED
