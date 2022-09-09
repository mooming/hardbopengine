// Created by mooming.go@gmail.com, 2022

#include "SystemAllocator.h"


#ifdef __UNIT_TEST__

#include "Log/Logger.h"
#include <iostream>
#include <vector>


bool HE::SystemAllocatorTest::DoTest()
{
	using namespace std;

	auto log = Logger::Get(GetName());

	for (int j = 0; j < 100; ++j)
	{
		vector<int, SystemAllocator<int>> hbVector;
		for (int i = 0; i < 100; ++i)
		{
			hbVector.push_back(i);
		}
	}

#ifdef __MEMORY_INVESTIGATION__

#ifdef __MEMORY_BUFFER_UNDERRUN_CHECK__
	{
		SystemAllocator<uint8_t> allocator;

		const size_t requestedSize = 1;
		uint8_t* buffer = allocator.allocate(requestedSize);
		auto underRunPtr = buffer - 1;
		auto overRunPtr = buffer + requestedSize;

		log.Out([&](auto& ls)
		{
			ls << "Unterrun Test: ptr = " << (void*)buffer
				<< ", under-run ptr = " << underRunPtr
				<< ", over-run ptr = " << overRunPtr;
		});

		*overRunPtr = 0;

		// Comment the following line to proceede to the next.
		*underRunPtr = 0;

		allocator.deallocate(buffer, requestedSize);
	}
#else // __MEMORY_BUFFER_UNDERRUN_CHECK__
	{
		SystemAllocator<uint8_t> allocator;

		const size_t requestedSize = 1;
		uint8_t* buffer = allocator.allocate(requestedSize);
		auto underRunPtr = buffer - 1;
		auto overRunPtr = buffer + requestedSize;

		log.Out([&](auto& ls)
			{
				ls << "Overrun Test: ptr = " << (void*)buffer
					<< ", under-run ptr = " << underRunPtr
					<< ", over-run ptr = " << overRunPtr;
			});

		*underRunPtr = 0;

		// Comment the following line to proceede to the next.
		*overRunPtr = 0;

		allocator.deallocate(buffer, requestedSize);
	}
#endif // __MEMORY_BUFFER_UNDERRUN_CHECK__

	{
		SystemAllocator<uint8_t> allocator;

		const size_t requestedSize = 1;
		uint8_t* buffer = allocator.allocate(requestedSize);
		
		*buffer = 0;

		log.Out([&](auto& ls)
			{
				ls << "Overrun Test: ptr = " << (void*)buffer << ", value = " << (int)(*buffer);
			});

		allocator.deallocate(buffer, requestedSize);

		// Comment the following lines to proceede to the next.
		*buffer = 1;
	}

#endif // __MEMORY_INVESTIGATION__

	return true;
}

#endif // __UNIT_TEST__
