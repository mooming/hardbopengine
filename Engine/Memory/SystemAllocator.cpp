// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "SystemAllocator.h"


#ifdef __UNIT_TEST__
#include <vector>

void hbe::SystemAllocatorTest::Prepare()
{
	AddTest("Vector Growth", [](auto&)
	{
		for (int j = 0; j < 100; ++j)
		{
			std::vector<int, SystemAllocator<int>> hbVector;

			for (int i = 0; i < 100; ++i)
			{
				hbVector.push_back(i);
			}
		}
	});

#if MEMORY_INVESTIGATOR_TEST_ENABLED
#if MEMORY_INVESTIGATION_ENABLED
#if MEMORY_BUFFER_UNDERRUN_CHECK_ENABLED

	AddTest("Buffer Under-run Detection", [this](auto& ls)
	{
		SystemAllocator<uint8_t> allocator;

		const size_t requestedSize = 1;
		uint8_t* buffer = allocator.allocate(requestedSize);

		[[maybe_unused]]
		auto underRunPtr = buffer - 1;

		auto overRunPtr = buffer + requestedSize;

		ls << "Underrun Test: ptr = " << (void*) buffer << lf;

		*overRunPtr = 0;

		ls << "A signal shall be posted from the next line"
		   << " due to buffer under-run/" << lf;
		*underRunPtr = 0; // Comment out to proceede to the next test.

		allocator.deallocate(buffer, requestedSize);
	});
#else // MEMORY_BUFFER_UNDERRUN_CHECK_ENABLED
	AddTest("Buffer Over-run Detection", [this](auto& ls)
	{
		SystemAllocator<uint8_t> allocator;

		const size_t requestedSize = 1;
		uint8_t* buffer = allocator.allocate(requestedSize);
		auto underRunPtr = buffer - 1;

		[[maybe_unused]]
		auto overRunPtr = buffer + requestedSize;

		ls << "Over-run Test: ptr = " << (void*) buffer << lf;

		*underRunPtr = 0;

		ls << "A signal shall be posted from the next line"
		   << " due to buffer over-run." << lf;
		*overRunPtr = 0; // Comment out to proceede to the next test.

		allocator.deallocate(buffer, requestedSize);
	});
#endif // MEMORY_BUFFER_UNDERRUN_CHECK_ENABLED

#if MEMORY_DANGLING_POINTER_CHECK_ENABLED
	AddTest("Dangling Pointer Detection", [this](auto& ls)
	{
		SystemAllocator<uint8_t> allocator;

		const size_t requestedSize = 1;
		uint8_t* buffer = allocator.allocate(requestedSize);

		*buffer = 0;

		ls << "Overrun Test: ptr = " << (void*) buffer << ", value = " << (int) (*buffer) << lf;

		allocator.deallocate(buffer, requestedSize);

		ls << "A signal shall be posted from the next line"
		   << " due to use-after-free." << lf;
		*buffer = 1; // Comment out to proceede to the next test.
	});
#endif // MEMORY_DANGLING_POINTER_CHECK_ENABLED
#endif // MEMORY_INVESTIGATION_ENABLED
#endif // MEMORY_INVESTIGATOR_TEST_ENABLED
}

#endif // __UNIT_TEST__
