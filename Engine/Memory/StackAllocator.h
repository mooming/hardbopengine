// Created by mooming.go@gmail.com

#pragma once

#include "AllocatorID.h"
#include "Config/BuildConfig.h"
#include "Core/Types.h"
#include "OSAL/SourceLocation.h"

namespace hbe
{
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
		static size_t GetSize(Pointer) { return 0; }

#if PROFILE_ENABLED
		StackAllocator(const char* name, SizeType capacity, const TSrcLoc& location = TSrcLoc::current());
#else // PROFILE_ENABLED
		StackAllocator(const char* name, SizeType capacity);
#endif // PROFILE_ENABLED

		~StackAllocator();

		Pointer Allocate(size_t size);
		void Deallocate(Pointer ptr, SizeType size);

		[[nodiscard]] size_t GetAvailable() const;
		[[nodiscard]] size_t GetUsage() const;
		[[nodiscard]] auto GetID() const { return id; }

	private:
		bool IsMine(Pointer ptr) const;
	};
} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{

	class StackAllocatorTest : public TestCollection
	{
	public:
		StackAllocatorTest() : TestCollection("StackAllocatorTest") {}

	protected:
		void Prepare() override;
	};

} // namespace hbe
#endif //__UNIT_TEST__
