// Created by mooming.go@gmail.com 2022

#include "OSMemory.h"

#ifdef PLATFORM_OSX
#include "Intrinsic.h"
#include "System/Debug.h"
#include <cerrno>
#include <iostream>
#include <malloc/malloc.h>
#include <sys/mman.h>
#include <unistd.h>

size_t OS::GetAllocSize(void* ptr)
{
    return malloc_size(ptr);
}

size_t OS::GetPageSize()
{
    static size_t pageSize = sysconf(_SC_PAGESIZE);
    return pageSize;
}

void* OS::VirtualAlloc(size_t size)
{
    auto ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    return ptr;
}

void OS::VirtualFree(void* address, std::size_t)
{
    free(address);
}

void OS::ProtectMemory(void* address, size_t n)
{
    auto result = mprotect(address, n, PROT_NONE);

    if (unlikely(result != 0))
    {
        using namespace std;
        cerr << "[OS::ProtectMemory] address = " << address << ", n = " << n
             << " : " << strerror(errno) << endl;

        HE::Assert(false);
    }
}

#endif // PLATFORM_OSX
