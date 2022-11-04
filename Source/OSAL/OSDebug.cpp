// Created by mooming.go@gmail.com 2022

#include "OSDebug.h"

#include "String/StringBuilder.h"


using namespace HE;

namespace OS
{

#ifdef __linux__
StaticString GetBackTrace()
{
    return StaticString("Not Implemented");
}

#elif defined __APPLE__
#include <execinfo.h>
#include <stdio.h>

StaticString GetBackTrace()
{
    InlineStringBuilder<8192> str;

    void* callstack[128];

    int frames = backtrace(callstack, 128);
    char** strs = backtrace_symbols(callstack, frames);

    str << "CallStack:\n";
    for (int i = 0; i < frames; ++i)
    {
        str << strs[i] << '\n';
    }

    free(strs);

    return StaticString(str.c_str());
}

#elif defined _WIN32
StaticString GetBackTrace()
{
    return StaticString("Not Implemented");
}
#else
static_assert(false, "System is not specified.");
#endif

} // OS

#ifdef __UNIT_TEST__


void HE::OSDebugTest::Prepare()
{
    AddTest("Print CallStack", [this](auto& ls)
    {
        auto callstack = OS::GetBackTrace();
        ls << callstack.c_str() << lf;
    });
}

#endif //__UNIT_TEST__
