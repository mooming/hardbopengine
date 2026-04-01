// Created by mooming.go@gmail.com 2022

#include "OSMemory.h"

// Ensure this file only compiles on macOS platform
static_assert(PLATFORM_OSX == 1, "OSXMemory.cpp should only be compiled on macOS platform");

#ifdef PLATFORM_OSX
#include <cerrno>
#include <iostream>
#include <malloc/malloc.h>
#include <sys/mman.h>
#include <unistd.h>
#include "Core/Debug.h"
#include "Intrinsic.h"

size_t OS::GetAllocSize(void* ptr) { return malloc_size(ptr); }

size_t OS::GetPageSize()
{
	static size_t pageSize = sysconf(_SC_PAGESIZE);
	return pageSize;
}

void* OS::VirtualAlloc(size_t size)
{
	auto ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	return ptr;
}

// BUG FIX: Memory allocated with mmap() must be freed with munmap(), not free()
// Using free() on mmap'd memory causes undefined behavior and heap corruption
void OS::VirtualFree(void* address, std::size_t size) { munmap(address, size); }

bool OS::IsValidAllocation(void* ptr)
{
	return ptr != nullptr && ptr != reinterpret_cast<void*>(-1);
}

void OS::ProtectMemory(void* address, size_t n)
{
	auto result = mprotect(address, n, PROT_NONE);

	if (unlikely(result != 0))
	{
		using namespace std;
		cerr << "[OS::ProtectMemory] address = " << address << ", n = " << n << " : " << strerror(errno) << endl;

		hbe::Assert(false);
	}
}

#endif // PLATFORM_OSX
