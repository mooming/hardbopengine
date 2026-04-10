// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstddef>
#include <cstdint>
#include "AllocatorID.h"

namespace hbe
{
	/// @brief Monotonic allocator that allocates linearly from a buffer.
	/// @details Simple bump-pointer allocator. Memory is released all at once
	/// on destruction. Individual deallocations are ignored.
	class MonotonicAllocator final
	{
	public:
		using TSize = size_t;
		using TPointer = void*;

	private:
		TAllocatorID id;
		TAllocatorID parentID;

		TSize cursor;
		TSize capacity;

		union
		{
			uint8_t* buffer;
			TPointer bufferPtr;
		};

	public:
		MonotonicAllocator(const char* name, TSize capacity);
		~MonotonicAllocator();

		TPointer Allocate(size_t size);
		void Deallocate(const TPointer ptr, TSize size);

		size_t GetAvailable() const;
		size_t GetUsage() const;

		inline auto GetID() const { return id; }

	private:
		bool IsMine(const TPointer ptr) const;
	};
} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{

	class MonotonicAllocatorTest : public TestCollection
	{
	public:
		MonotonicAllocatorTest() : TestCollection("MonotonicAllocatorTest") {}

	protected:
		virtual void Prepare() override;
	};

} // namespace hbe
#endif //__UNIT_TEST__
