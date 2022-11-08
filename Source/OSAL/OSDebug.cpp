// Created by mooming.go@gmail.com 2022

#include "OSDebug.h"

#include "HSTL/HString.h"
#include "HSTL/HVector.h"
#include "String/StringBuilder.h"
#include "String/StringUtil.h"


using namespace HE;

#ifdef __linux__
StaticString OS::GetBackTrace(uint16_t, uint16_t)
{
    return StaticString("Not Implemented");
}

#elif defined __APPLE__
#include "String/InlineStringBuilder.h"
#include <cstdio>
#include <cstring>
#include <cxxabi.h>
#include <execinfo.h>


StaticString OS::GetBackTrace(uint16_t startIndex, uint16_t maxDepth )
{
    constexpr size_t BufferSize = 8192;
    constexpr size_t LineBufferSize = 2048;
    constexpr size_t MaxCallStack = 128;

    ++startIndex;

    InlineStringBuilder<BufferSize> strBuild;
    void* callstack[MaxCallStack];

    int frames = backtrace(callstack, MaxCallStack);
    char** strs = backtrace_symbols(callstack, frames);


    strBuild << "CallStack:\n";
    for (int i = startIndex; i < frames && maxDepth > 0; ++i, --maxDepth)
    {
        int tokenIndex = 0;
        std::string_view tokens[5];

        auto PerToken = [&tokenIndex, &tokens](std::string_view token)
        {
            if (likely(tokenIndex < 5))
            {
                tokens[tokenIndex] = token;
            }

            ++tokenIndex;
        };

        StringUtil::ForEachToken(strs[i], PerToken, " \t\n\r+");

        if (tokenIndex == 5)
        {
            InlineStringBuilder<LineBufferSize> mangledName;
            mangledName << tokens[3];

            int status = 0;
            auto name = abi::__cxa_demangle(mangledName.c_str()
                , nullptr, nullptr, &status);

            strBuild << (i - 1) << ": [" << tokens[1] << "] " << tokens[2]
                << ' ' << name << " +" << tokens[4] << '\n';

            free(name);
        }
        else
        {
            strBuild << strs[i] << '\n';
        }
    }

    Assert(strs != nullptr);
    free(strs);

    return StaticString(strBuild.c_str());
}

#elif defined _WIN32
StaticString OS::GetBackTrace(uint16_t)
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
