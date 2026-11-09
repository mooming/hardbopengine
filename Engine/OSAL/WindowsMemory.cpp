// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "OSMemory.h"

#if PLATFORM_WINDOWS
#include <errhandlingapi.h>
#include <iostream>
#include <malloc.h>
#include <memoryapi.h>
#include <sysinfoapi.h>
#include <windows.h>

#include "Core/CommonMacros.h"

size_t OS::GetAllocSize(void* ptr) noexcept
{
	void* raw = *(reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(ptr) - sizeof(void*)));

	const auto allocSize = _msize(raw);

	return allocSize;
}

size_t OS::GetPageSize() noexcept
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

void* OS::VirtualAlloc(size_t size)
{
	return ::VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

void OS::VirtualFree(void* address, std::size_t n) noexcept
{
	auto result = ::VirtualFree(address, 0, MEM_RELEASE);
	if (unlikely(!result))
	{
		hbe::Assert(false);
	}
}

bool OS::IsValidAllocation(void* ptr) noexcept
{
	return ptr != nullptr;
}

void OS::ProtectMemory(void* address, size_t n) noexcept
{
	DWORD oldProtect = 0;
	auto result = VirtualProtect(address, n, PAGE_NOACCESS, &oldProtect);

	ReturnIf(likely(result));

	using namespace std;
	auto errorId = GetLastError();

	cerr << "[OS::ProtectMemory] address = " << address << ", n = " << n << " : error code = " << errorId << endl;

	hbe::Assert(false);
}
#endif // PLATFORM_WINDOWS
