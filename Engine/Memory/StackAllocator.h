// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "AllocatorID.h"
#include "Config/BuildConfig.h"
#include "Core/Types.h"
#include "OSAL/SourceLocation.h"

namespace hbe
{
/// @brief Stack-based allocator for temporary allocations.
/// @details Bump-pointer allocator over a fixed-size buffer. Deallocate IS supported, but
///          strictly LIFO: the most recently allocated block must be released first, with the
///          same byte count it was allocated with (the size is rounded up to
///          `Config::DefaultAlign` on both paths). A release that does not match the current
///          top of stack is logged and asserted rather than silently rewinding the cursor.
///          A pointer belonging to another allocator is forwarded to the parent allocator
///          instead of failing. Whatever is still allocated when the stack is destroyed is
///          released as a group.
class StackAllocator final
{
public:
	using This = StackAllocator;
	using SizeType = size_t;

#if PROFILE_ENABLED
	using TSrcLoc = hbe::source_location;
#endif // PROFILE_ENABLED

private:
	TAllocatorID id;
	TAllocatorID parentID;

	SizeType capacity;
	SizeType cursor;

	union
	{
		Byte* buffer;
		Pointer bufferPtr;
	};

#if PROFILE_ENABLED
	TSrcLoc srcLocation;
#endif // PROFILE_ENABLED

public:
	// Not Supported
	static size_t GetSize(Pointer)
	{
		return 0;
	}

#if PROFILE_ENABLED
	StackAllocator(const char* name, SizeType capacity, const TSrcLoc& location = TSrcLoc::current());
#else // PROFILE_ENABLED
	StackAllocator(const char* name, SizeType capacity);
#endif // PROFILE_ENABLED

	~StackAllocator();

	[[nodiscard]] Pointer Allocate(size_t size);
	void Deallocate(Pointer ptr, SizeType size) noexcept;

	[[nodiscard]] size_t GetAvailable() const;
	[[nodiscard]] size_t GetUsage() const;

	[[nodiscard]] auto GetID() const
	{
		return id;
	}

private:
	bool IsMine(Pointer ptr) const;
};
} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{

/// @brief Test class for StackAllocator.
class StackAllocatorTest : public TestCollection
{
public:
	StackAllocatorTest()
		: TestCollection("StackAllocatorTest")
	{
	}

protected:
	void Prepare() override;
};

} // namespace hbe
#endif //__UNIT_TEST__
