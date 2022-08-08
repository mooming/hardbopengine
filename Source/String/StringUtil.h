// Created by mooming.go@gmail.com 2016

#pragma once

#include "HSTL/HString.h"


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
TString PrettyFunctionToClassName(const char* PrettyFunction);
TString PrettyFunctionToFunctionName(const char* PrettyFunction);

} // StringUtil

#ifdef __UNIT_TEST__
#include "Test/TestCase.h"

namespace HE
{

class StringUtilTest : public TestCase
{
public:
    StringUtilTest() : TestCase("StringUtilTest") {}
    
protected:
    virtual bool DoTest() override;
};

} // HE
#endif //__UNIT_TEST__
