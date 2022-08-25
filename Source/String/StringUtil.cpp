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

HE::StaticString PrettyFunctionToFunctionName(const char* PrettyFunction)
{
    using TStr = HSTL::HInlineString<256>;
    TStr str(PrettyFunction);
    
    auto start = str.find_last_of("::") + 1;
    if (start == TStr::npos)
    {
        return HE::StaticString(PrettyFunction);
    }
    
    str = str.substr(start);
    
    return HE::StaticString(str.c_str());
}

HE::StaticString PrettyFunctionToClassName(const char* PrettyFunction)
{
    using TStr = HSTL::HInlineString<256>;
    TStr str(PrettyFunction);
   
    auto end = str.find_last_of("::") - 1;
    if (end == TStr::npos)
        return HE::StaticString();
    
    str.resize(end);
    auto start = str.find_last_of(" ") + 1;
    str = str.substr(start, (end - start));
    
    return HE::StaticString(str.c_str());
}

HE::StaticString PrettyFunctionToMethodName(const char* PrettyFunction)
{
    using TStr = HSTL::HInlineString<256>;
    TStr str(PrettyFunction);
    
    auto start = str.find_last_of("::");
    if (start == TStr::npos)
        return HE::StaticString(PrettyFunction);
    
    start = str.find_last_of(" ", start - 2) + 1;
    str = str.substr(start);
    
    return HE::StaticString(str.c_str());
}

HE::StaticString PrettyFunctionToCompactClassName(const char* PrettyFunction)
{
    using TStr = HSTL::HInlineString<256>;
    TStr str(PrettyFunction);
   
    auto end = str.find_last_of("::") - 1;
    if (end == TStr::npos)
        return HE::StaticString();
    
    str.resize(end);
    auto start = str.find_last_of("::") + 1;
    str = str.substr(start, (end - start));
    
    return HE::StaticString(str.c_str());
}

HE::StaticString PrettyFunctionToCompactMethodName(const char* PrettyFunction)
{
    using TStr = HSTL::HInlineString<256>;
    TStr str(PrettyFunction);
    
    auto start = str.find_last_of("::");
    if (start == TStr::npos)
        return HE::StaticString();
    start = str.find_last_of("::", start - 2) + 1;
    str = str.substr(start);
    
    return HE::StaticString(str.c_str());
}

size_t CalculateHash(const char* text)
{
    size_t hashCode = 5381;
    
    while (*text != '\0')
    {
        size_t ch = *text;
        ++text;
        hashCode = ((hashCode << 5) + hashCode) + ch; /* hash * 33 + c */
    }
    
    return hashCode;
}

} // StringUtil

#ifdef __UNIT_TEST__
#include "System/Time.h"
#include <iostream>


namespace HE
{
bool StringUtilTest::DoTest()
{
    using namespace StringUtil;
    
    using namespace std;
    
    int testCount = 0;
    int failCount = 0;
    
    {
        StaticString className("HE::StringUtilTest");
        cout << "[" << className << "] Test PrettyFunctionToClassName" << endl;
        
        auto name = PrettyFunctionToClassName(__PRETTY_FUNCTION__);
        cout << "[" << className << "] Class Name is " << name
            << " / " << className << endl;

        if (name != className)
        {
            cerr << "[Fail]" << endl;
            ++failCount;
        }
        else
        {
            cout << "[PASS]" << endl;
        }
    }
    
    ++testCount;
    
    {
        StaticString className("StringUtilTest");
        cout << "[" << className << "] Test PrettyFunctionToCompactClassName" << endl;
        
        auto name = PrettyFunctionToCompactClassName(__PRETTY_FUNCTION__);
        cout << "[" << className << "] Compact Class Name is " << name
            << " / " << className << endl;

        if (name != className)
        {
            cerr << "[Fail]" << endl;
            ++failCount;
        }
        else
        {
            cout << "[PASS]" << endl;
        }
    }
    
    ++testCount;
    
    auto className = PrettyFunctionToClassName(__PRETTY_FUNCTION__);
    
    {
        cout << "[" << className << "] Test PrettyFunctionToFunctionName" << endl;
        
        StaticString funcName("DoTest()");
        auto name = PrettyFunctionToFunctionName(__PRETTY_FUNCTION__);
        cout << "[" << className << "] Function Name is "
            << name  << " / " << funcName << endl;
        
        if (name != funcName)
        {
            cerr << "[Fail]" << endl;
            ++failCount;
        }
        else
        {
            cout << "[PASS]" << endl;
        }
    }
    
    ++testCount;
    
    {
        cout << "[" << className << "] Test PrettyFunctionToMethodName" << endl;
        
        StaticString funcName("HE::StringUtilTest::DoTest()");
        auto name = PrettyFunctionToMethodName(__PRETTY_FUNCTION__);
        cout << "[" << className << "] Function Name is "
            << name  << " / " << funcName << endl;
        
        if (name != funcName)
        {
            cerr << "[Fail]" << endl;
            ++failCount;
        }
        else
        {
            cout << "[PASS]" << endl;
        }
    }
    
    ++testCount;
    
    {
        cout << "[" << className << "] Test PrettyFunctionToCompactMethodName" << endl;
        
        StaticString funcName("StringUtilTest::DoTest()");
        auto name = PrettyFunctionToCompactMethodName(__PRETTY_FUNCTION__);
        cout << "[" << className << "] Function Name is "
            << name  << " / " << funcName << endl;
        
        if (name != funcName)
        {
            cerr << "[Fail]" << endl;
            ++failCount;
        }
        else
        {
            cout << "[PASS]" << endl;
        }
    }
    
    ++testCount;
    
    return failCount < 1;
}
} // HE
#endif //__UNIT_TEST__
