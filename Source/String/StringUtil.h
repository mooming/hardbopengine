// Created by mooming.go@gmail.com 2016

#pragma once

#include "HSTL/HString.h"
#include "String/StaticString.h"


namespace StringUtil
{

using TString = HSTL::HString;

TString Trim(const TString& str);
TString TrimPath(const TString& path);
TString ToLowerCase(TString src);
bool EqualsIgnoreCase(const TString& a, const TString& b);
bool StartsWith(const TString& src, const TString& startTerm);
bool StartsWithIgnoreCase(const TString& src, const TString& startTerm);
bool EndsWith(const TString& src, const TString& endTerm);
bool EndsWithIgnoreCase(const TString& src, const TString& endTerm);
TString PathToName(const TString& path);

HE::StaticString PrettyFunctionToFunctionName(const char* PrettyFunction);
HE::StaticString PrettyFunctionToClassName(const char* PrettyFunction);
HE::StaticString PrettyFunctionToMethodName(const char* PrettyFunction);
HE::StaticString PrettyFunctionToCompactClassName(const char* PrettyFunction);
HE::StaticString PrettyFunctionToCompactMethodName(const char* PrettyFunction);

size_t StrLen(const char* text, size_t bufferSize);
size_t CalculateHash(const char* text);

} // StringUtil

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace HE
{

class StringUtilTest : public TestCollection
{
public:
    StringUtilTest() : TestCollection("StringUtilTest") {}
    
protected:
    virtual void Prepare() override;
};

} // HE
#endif //__UNIT_TEST__
