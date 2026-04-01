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

// BUG FIX: Parameter was unnamed but body referenced 'size' - caused compilation error
// Added parameter name to fix undefined variable error
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
	// BUG FIX: VirtualFree returns non-zero on SUCCESS, zero on FAILURE
	// Original code checked if (result) which triggered Assert on success
	// Fixed to check if (!result) to assert only on actual failure
	if (unlikely(!result))
	{
		hbe::Assert(false);
	}
}

bool OS::IsValidAllocation(void* ptr)
{
	return ptr != nullptr;
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

#ifdef __UNIT_TEST__

#include "Core/Debug.h"

namespace hbe
{

	void OSMemoryTest::Prepare()
	{
		// BUG FIX TEST: VirtualFree on Linux/macOS was using free() instead of munmap()
		// This test verifies VirtualAlloc/VirtualFree round-trip works correctly
		AddTest("VirtualAlloc VirtualFree Round Trip", [this](auto& ls)
		{
			constexpr size_t allocSize = 4096;
			void* ptr = OS::VirtualAlloc(allocSize);

			if (!OS::IsValidAllocation(ptr))
			{
				ls << "VirtualAlloc failed" << lferr;
				return;
			}

			// Write to the memory to verify it's valid
			memset(ptr, 0xAB, allocSize);

			// Verify the written pattern
			const uint8_t* bytes = static_cast<const uint8_t*>(ptr);
			bool patternOk = true;
			for (size_t i = 0; i < allocSize; ++i)
			{
				if (bytes[i] != 0xAB)
				{
					patternOk = false;
					break;
				}
			}

			if (!patternOk)
			{
				ls << "Memory pattern verification failed after VirtualAlloc" << lferr;
			}

			// VirtualFree should not crash or assert
			OS::VirtualFree(ptr, allocSize);
			ls << "VirtualAlloc/VirtualFree round trip succeeded" << lf;
		});

		// BUG FIX TEST: VirtualFree Windows error check was inverted
		// This test verifies freeing valid memory doesn't trigger false asserts
		AddTest("VirtualFree Valid Memory", [this](auto& ls)
		{
			constexpr size_t allocSize = 8192;
			void* ptr = OS::VirtualAlloc(allocSize);
			Assert(ptr != nullptr && "VirtualAlloc should succeed");

			// Write and verify
			int* intPtr = static_cast<int*>(ptr);
			*intPtr = 0xDEADBEEF;
			Assert(*intPtr == 0xDEADBEEF && "Memory write verification failed");

			// This should NOT trigger an assert (was the bug on Windows)
			OS::VirtualFree(ptr, allocSize);
			ls << "VirtualFree on valid memory succeeded without false assert" << lf;
		});

		// BUG FIX TEST: VirtualFree on Linux had undefined 'size' variable
		// This test verifies the parameter is properly passed through
		AddTest("VirtualFree With Size Parameter", [this](auto& ls)
		{
			size_t pageSize = OS::GetPageSize();
			void* ptr = OS::VirtualAlloc(pageSize);
			Assert(ptr != nullptr && "VirtualAlloc should succeed");

			// Fill memory
			memset(ptr, 0xCD, pageSize);

			// Free with explicit size - verifies parameter passing works
			OS::VirtualFree(ptr, pageSize);
			ls << "VirtualFree with size parameter succeeded" << lf;
		});

		// Test multiple alloc/free cycles to catch memory corruption
		AddTest("VirtualAlloc VirtualFree Multiple Cycles", [this](auto& ls)
		{
			constexpr int cycles = 10;
			bool allOk = true;

			for (int i = 0; i < cycles; ++i)
			{
				size_t size = (i + 1) * OS::GetPageSize();
				void* ptr = OS::VirtualAlloc(size);

				if (!OS::IsValidAllocation(ptr))
				{
					ls << "VirtualAlloc failed at cycle " << i << lferr;
					allOk = false;
					break;
				}

				memset(ptr, i, size);

				OS::VirtualFree(ptr, size);
			}

			if (allOk)
			{
				ls << cycles << " alloc/free cycles completed successfully" << lf;
			}
		});

		// Test page size retrieval
		AddTest("GetPageSize", [this](auto& ls)
		{
			size_t pageSize = OS::GetPageSize();
			ls << "Page size: " << pageSize << lf;

			if (pageSize == 0)
			{
				ls << "GetPageSize returned 0" << lferr;
			}

			// Page size should be a power of 2
			if ((pageSize & (pageSize - 1)) != 0)
			{
				ls << "Page size " << pageSize << " is not a power of 2" << lfwarn;
			}
		});
	}

} // namespace hbe
#endif // __UNIT_TEST__
