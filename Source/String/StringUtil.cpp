// Created by mooming.go@gmail.com 2016

#include "StringUtil.h"

#include <algorithm>
#include <memory>


using namespace HSTL;

namespace StringUtil
{

TString Trim(const TString& str)
{
    auto start = str.begin();
    auto end = str.end();
    
    while (start != end && std::isspace(*start))
    {
        ++start;
    }
    
    do
    {
        --end;
    } while (std::distance(start, end) > 0 && std::isspace(*end));
    
    return TString(start, end + 1);
}

TString TrimPath(const TString& path)
{
    char tmp[PATH_MAX + 1];
    auto cStr = path.c_str();
    
    for (size_t i = 0; i < path.length(); ++i)
    {
        if (cStr[i] == '\\')
        {
            tmp[i] = '/';
        }
        else
        {
            tmp[i] = cStr[i];
        }
    }
    
    if (path.length() > 2 && tmp[path.length() - 2] != '.' && tmp[path.length() - 1] == '/')
    {
        tmp[path.length() - 1] = '\0';
    }
    
    tmp[path.length()] = '\0';
    
    return TString(tmp);
}

TString ToLowerCase(TString src)
{
    auto ToLowerChar = [](char ch)
    {
        constexpr char delta = 'A' - 'a';
        
        if ('A' <= ch && ch <= 'Z')
        {
            ch -= delta;
        }
        
        return ch;
    };
    
    std::transform(src.begin(), src.end(), src.begin(), ToLowerChar);
    
    return src;
}

bool EqualsIgnoreCase(const TString& a, const TString& b)
{
    return ToLowerCase(a) == ToLowerCase(b);
}

bool StartsWith(const TString& src, const TString& startTerm)
{
    if (src.length() < startTerm.length())
        return false;
    
    TString head = src.substr(0, startTerm.length());
    
    return head == startTerm;
}

bool StartsWithIgnoreCase(const TString& src, const TString& startTerm)
{
    return StartsWith(ToLowerCase(src), ToLowerCase(startTerm));
}

bool EndsWith(const TString& src, const TString& endTerm)
{
    if (src.length() < endTerm.length())
        return false;
    
    const char* srcStr = src.c_str();
    TString last(srcStr + (src.length() - endTerm.length()));
    
    return last == endTerm;
}

bool EndsWithIgnoreCase(const TString& src, const TString& endTerm)
{
    return EndsWith(ToLowerCase(src), ToLowerCase(endTerm));
}

TString PathToName(const TString& path)
{
    auto length = path.size();
    using Index = decltype(length);
    
    auto buffer = path.c_str();
    
    bool found = false;
    Index lastIndex = 0;
    for (Index i = 0; i < length; ++i)
    {
        const char ch = buffer[i];
        if (ch == '/' || ch == '\\')
        {
            lastIndex = i;
            found = true;
        }
    }
    
    if (!found || lastIndex >= (length - 1))
    {
        return path;
    }
    
    return TString(buffer + lastIndex + 1);
}

TString PrettyFunctionToClassName(const char* PrettyFunction)
{
    return TString(PrettyFunction);
}

TString PrettyFunctionToFunctionName(const char* PrettyFunction)
{
    return TString(PrettyFunction);
}

} // HE

#ifdef __UNIT_TEST__
#include "System/Time.h"
#include <iostream>


namespace HE
{
bool StringUtilTest::DoTest()
{
    using namespace StringUtil;
    
    
    
    return true;
}
} // HE
#endif //__UNIT_TEST__
