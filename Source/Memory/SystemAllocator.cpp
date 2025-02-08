// Created by mooming.go@gmail.com

#include "SystemAllocator.h"

#ifdef __UNIT_TEST__
#include <vector>

void HE::SystemAllocatorTest::Prepare()
{
    AddTest("Vector Growth", [](auto&) {
        for (int j = 0; j < 100; ++j)
        {
            std::vector<int, SystemAllocator<int>> hbVector;

            for (int i = 0; i < 100; ++i)
            {
                hbVector.push_back(i);
            }
        }
    });

#ifdef __MEMORY_INVESTIGATOR_TEST__
#ifdef __MEMORY_INVESTIGATION__
#ifdef __MEMORY_BUFFER_UNDERRUN_CHECK__

    AddTest("Buffer Under-run Detection", [this](auto& ls) {
        SystemAllocator<uint8_t> allocator;

        const size_t requestedSize = 1;
        uint8_t* buffer = allocator.allocate(requestedSize);

        [[maybe_unused]]
        auto underRunPtr = buffer - 1;

        auto overRunPtr = buffer + requestedSize;

        ls << "Underrun Test: ptr = " << (void*)buffer << lf;

        *overRunPtr = 0;

        ls << "A signal shall be posted from the next line"
           << " due to buffer under-run/" << lf;
        *underRunPtr = 0; // Comment out to proceede to the next test.

        allocator.deallocate(buffer, requestedSize);
    });
#else  // __MEMORY_BUFFER_UNDERRUN_CHECK__
    AddTest("Buffer Over-run Detection", [this](auto& ls) {
        SystemAllocator<uint8_t> allocator;

        const size_t requestedSize = 1;
        uint8_t* buffer = allocator.allocate(requestedSize);
        auto underRunPtr = buffer - 1;

        [[maybe_unused]]
        auto overRunPtr = buffer + requestedSize;

        ls << "Over-run Test: ptr = " << (void*)buffer << lf;

        *underRunPtr = 0;

        ls << "A signal shall be posted from the next line"
           << " due to buffer over-run." << lf;
        *overRunPtr = 0; // Comment out to proceede to the next test.

        allocator.deallocate(buffer, requestedSize);
    });
#endif // __MEMORY_BUFFER_UNDERRUN_CHECK__

#ifdef __MEMORY_DANGLING_POINTER_CHECK__
    AddTest("Dangling Pointer Detection", [this](auto& ls) {
        SystemAllocator<uint8_t> allocator;

        const size_t requestedSize = 1;
        uint8_t* buffer = allocator.allocate(requestedSize);

        *buffer = 0;

        ls << "Overrun Test: ptr = " << (void*)buffer
           << ", value = " << (int)(*buffer) << lf;

        allocator.deallocate(buffer, requestedSize);

        ls << "A signal shall be posted from the next line"
           << " due to use-after-free." << lf;
        *buffer = 1; // Comment out to proceede to the next test.
    });
#endif // __MEMORY_DANGLING_POINTER_CHECK__
#endif // __MEMORY_INVESTIGATION__
#endif // __MEMORY_INVESTIGATOR_TEST__
}

#endif // __UNIT_TEST__
