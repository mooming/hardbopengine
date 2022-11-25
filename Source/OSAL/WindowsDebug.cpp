// Created by mooming.go@gmail.com 2022

#include "OSDebug.h"

#ifdef PLATFORM_WINDOWS
#include "String/InlineStringBuilder.h"
#include <cstdlib>
#include <Windows.h>
#include <DbgHelp.h>

#pragma comment(lib,"dbghelp.lib")


using namespace HE;

StaticString OS::GetBackTrace(uint16_t startIndex, uint16_t maxDepth)
{
    constexpr size_t BufferSize = 8192;
    constexpr size_t LineBufferSize = 2048;
    constexpr size_t MaxCallStack = 128;

    InlineStringBuilder<BufferSize> strBuild;

    HANDLE process = GetCurrentProcess();
    SymInitialize(process, nullptr, true);

    void* stack[MaxCallStack];
    auto frames = CaptureStackBackTrace(startIndex, MaxCallStack, stack, nullptr);

    SYMBOL_INFO* symbol = (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + LineBufferSize, 1);
    symbol->MaxNameLen = 255;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

    strBuild << "CallStack:\n";

    for (int i = 0; i < frames && i < maxDepth; ++i)
    {
        DWORD64 address = reinterpret_cast<DWORD64>(stack[i]);
        SymFromAddr(process, address, 0, symbol);

        strBuild << i << " : "
            << static_cast<const char*>(symbol->Name) << " ("
            << (void*)symbol->Address << ")\n";
    }

    free(symbol);

    return StaticString(strBuild.c_str());
}

#endif // PLATFORM_WINDOWS
