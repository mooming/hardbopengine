// Created by mooming.go@gmail.com 2016

#pragma once

#include <functional>
#include <string_view>
#include "HSTL/HString.h"
#include "HSTL/HVector.h"
#include "String/StaticString.h"

namespace StringUtil
{

	template<typename T>
	using TVector = HSTL::HVector<T>;
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
	void ForEachToken(const char* str, const std::function<void(std::string_view)> func,
					  const char* separators = " \t\n\r");

	hbe::StaticString ToFunctionName(const char* PrettyFunction);
	hbe::StaticString ToClassName(const char* PrettyFunction);
	hbe::StaticString ToMethodName(const char* PrettyFunction);
	hbe::StaticString ToCompactClassName(const char* PrettyFunction);
	hbe::StaticString ToCompactMethodName(const char* PrettyFunction);

	size_t StrLen(const char* text);
	size_t StrLen(const char* text, size_t bufferSize);
	const char* StrCopy(char* dst, const char* src, size_t n);
	size_t CalculateHash(const char* text);
	size_t CalculateHash(const std::string_view& str);
} // namespace StringUtil

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{

	class StringUtilTest : public TestCollection
	{
	public:
		StringUtilTest() : TestCollection("StringUtilTest") {}

	protected:
		virtual void Prepare() override;
	};

} // namespace hbe
#endif //__UNIT_TEST__
