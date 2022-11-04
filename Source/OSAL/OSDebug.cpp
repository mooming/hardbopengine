// Created by mooming.go@gmail.com 2022

#include "OSDebug.h"

#include "HSTL/HString.h"
#include "HSTL/HVector.h"
#include "String/StringBuilder.h"
#include "String/StringUtil.h"


using namespace HE;

#ifdef __linux__
StaticString OS::GetBackTrace()
{
    return StaticString("Not Implemented");
}

#elif defined __APPLE__
#include "HSTL/HString.h"
#include "String/String.h"
#include <cxxabi.h>
#include <execinfo.h>
#include <stdio.h>


StaticString OS::GetBackTrace()
{
    constexpr size_t InlineBufferSize = 8192;
    InlineStringBuilder<InlineBufferSize> str;
    str.Reserve(InlineBufferSize - 1);

    void* callstack[128];

    int frames = backtrace(callstack, 128);
    char** strs = backtrace_symbols(callstack, frames);

    str << "CallStack:\n";
    for (int i = 1; i < frames; ++i)
    {
        HSTL::HVector<HSTL::HString> tokens;
        StringUtil::Tokenize(tokens, strs[i], " \t\n\r+");

        if (tokens.size() == 5)
        {
            int status = 0;
            auto mangledName = tokens[3].c_str();
            auto name = abi::__cxa_demangle(mangledName, nullptr
                , nullptr, &status);

            str << (i - 1) << ": [" << tokens[1] << "] " << tokens[2]
                << ' ' << name << " +" << tokens[4] << '\n';

            free(name);
        }
        else
        {
            str << strs[i] << '\n';
        }
    }

    Assert(strs != nullptr);
    free(strs);

    return StaticString(str.c_str());
}

#elif defined _WIN32
StaticString OS::GetBackTrace()
{
    return StaticString("Not Implemented");
}
#else
static_assert(false, "System is not specified.");
#endif

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
