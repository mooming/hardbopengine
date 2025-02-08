// Created by mooming.go@gmail.com 2022

#include "OSMemory.h"

#ifdef PLATFORM_WINDOWS
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
    auto GetPageSizeWindows = []() {
        SYSTEM_INFO sSysInfo;
        GetSystemInfo(&sSysInfo);
        return sSysInfo.dwPageSize;
    };

    static size_t pageSize = GetPageSizeWindows();

    return pageSize;
}

void* OS::VirtualAlloc(size_t size)
{
    return ::VirtualAlloc(
        nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

void OS::VirtualFree(void* address, std::size_t n)
{
    auto result = ::VirtualFree(address, n, MEM_RELEASE);
    if (unlikely(result))
    {
        HE::Assert(false);
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

    auto& engine = HE::Engine::Get();
    engine.LogError([address, n, errorId](auto& log) {
        log << "[OS::ProtectMemory] address = " << address << ", n = " << n
            << " : error code = " << errorId << endl;
    });

    HE::Assert(false);
}
#endif // PLATFORM_WINDOWS
