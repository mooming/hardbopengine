// Created by mooming.go@gmail.com 2022

#include "OSMemory.h"

#include "../Engine/Engine.h"
#include "Core/Debug.h"
#include "Intrinsic.h"

#ifdef __linux__
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <malloc.h>
#include <sys/mman.h>
#include <unistd.h>

size_t OS::GetAllocSize(void* ptr) { return malloc_usable_size(ptr); }

size_t OS::GetPageSize() { return sysconf(_SC_PAGESIZE); }

void* OS::VirtualAlloc(size_t size)
{
	auto ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	return ptr;
}

void OS::VirtualFree(void* address, std::size_t size) { munmap(address, size); }

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

#elif defined __APPLE__
#include <cerrno>
#include <iostream>
#include <malloc/malloc.h>
#include <sys/mman.h>
#include <unistd.h>

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

void OS::VirtualFree(void* address, std::size_t size) { munmap(address, size); }

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

#elif defined _WIN32
#include <errhandlingapi.h>
#include <malloc.h>
#include <memoryapi.h>
#include <sysinfoapi.h>
#include <windows.h>

size_t OS::GetAllocSize(void* ptr)
{
	const auto allocSize = _msize(ptr);
	return allocSize;
}

size_t OS::GetPageSize()
{
	auto GetPageSizeWindows = []()
	{
		SYSTEM_INFO sSysInfo;
		GetSystemInfo(&sSysInfo);
		return sSysInfo.dwPageSize;
	};

	static size_t pageSize = GetPageSizeWindows();

	return pageSize;
}

void* OS::VirtualAlloc(size_t size) { return ::VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE); }

void OS::VirtualFree(void* address, std::size_t n)
{
	auto result = ::VirtualFree(address, n, MEM_RELEASE);
	if (unlikely(result))
	{
		hbe::Assert(false);
	}
}

void OS::ProtectMemory(void* address, size_t n)
{
	DWORD oldProtect = 0;
	auto result = VirtualProtect(address, n, PAGE_NOACCESS, &oldProtect);

	if (likely(result))
	{
		return;
	}

	using namespace std;
	auto errorId = GetLastError();

	auto& engine = hbe::Engine::Get();
	engine.LogError([address, n, errorId](auto& log)
	{ log << "[OS::ProtectMemory] address = " << address << ", n = " << n << " : error code = " << errorId << endl; });

	hbe::Assert(false);
}

#else
static_assert(false, "System is not specified.");
#endif
