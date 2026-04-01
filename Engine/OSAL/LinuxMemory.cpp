// Created by mooming.go@gmail.com 2022

#include "OSMemory.h"

// Ensure this file only compiles on Linux platform
static_assert(PLATFORM_LINUX == 1, "LinuxMemory.cpp should only be compiled on Linux platform");

#ifdef PLATFORM_LINUX
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <malloc.h>
#include <sys/mman.h>
#include <unistd.h>
#include "Core/Debug.h"
#include "Intrinsic.h"

size_t OS::GetAllocSize(void* ptr) { return malloc_usable_size(ptr); }

size_t OS::GetPageSize() { return sysconf(_SC_PAGESIZE); }

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

#endif // PLATFORM_LINUX
