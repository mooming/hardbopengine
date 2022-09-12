// Created by mooming.go@gmail.com, 2022

#include "SystemAllocator.h"


#ifdef __UNIT_TEST__

#include "Log/Logger.h"
#include <iostream>
#include <vector>


bool HE::SystemAllocatorTest::DoTest()
{
	using namespace std;

    int testCount = 0;
    int errorCount = 0;
    
	auto log = Logger::Get(GetName());

    {
        log.Out([testCount](auto& ls)
        {
            ls << '[' << testCount << "] Vector Growth Test";
        });
        
        for (int j = 0; j < 100; ++j)
        {
            vector<int, SystemAllocator<int>> hbVector;
            for (int i = 0; i < 100; ++i)
            {
                hbVector.push_back(i);
            }
        }
    }
    
#ifdef __MEMORY_INVESTIGATOR_TEST__
#ifdef __MEMORY_INVESTIGATION__
#ifdef __MEMORY_BUFFER_UNDERRUN_CHECK__
    ++testCount;
    
	{
		SystemAllocator<uint8_t> allocator;

		const size_t requestedSize = 1;
		uint8_t* buffer = allocator.allocate(requestedSize);
        
        [[maybe_unused]]
		auto underRunPtr = buffer - 1;
        
		auto overRunPtr = buffer + requestedSize;

		log.Out([&](auto& ls)
		{
            ls << "Unterrun Test: ptr = " << (void*)buffer;
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
        
        [[maybe_unused]]
		auto overRunPtr = buffer + requestedSize;

		log.Out([&](auto& ls)
        {
            ls << "Overrun Test: ptr = " << (void*)buffer;
        });

		*underRunPtr = 0;

		// Comment the following line to proceede to the next.
		*overRunPtr = 0;

		allocator.deallocate(buffer, requestedSize);
	}
#endif // __MEMORY_BUFFER_UNDERRUN_CHECK__

#ifdef __MEMORY_DANGLING_POINTER_CHECK__
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
#endif // __MEMORY_DANGLING_POINTER_CHECK__
#endif // __MEMORY_INVESTIGATION__
#endif // __MEMORY_INVESTIGATOR_TEST__
    
	return errorCount <= 0;
}

#endif // __UNIT_TEST__
