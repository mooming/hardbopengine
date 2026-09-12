// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <functional>
#include <string_view>
#include "HSTL/HString.h"
#include "HSTL/HVector.h"
#include "String/StaticString.h"

namespace hbe { namespace StringUtil
{

	template<typename T>
	using TVector = HVector<T>;
	using TString = HString;

	[[nodiscard]] TString Trim(const TString& str);
	[[nodiscard]] TString TrimPath(const TString& path);
	[[nodiscard]] TString ToLowerCase(const TString& src);
	[[nodiscard]] bool EqualsIgnoreCase(const TString& a, const TString& b);
	[[nodiscard]] bool StartsWith(const TString& src, const TString& startTerm);
	[[nodiscard]] bool StartsWithIgnoreCase(const TString& src, const TString& startTerm);
	[[nodiscard]] bool EndsWith(const TString& src, const TString& endTerm);
	[[nodiscard]] bool EndsWithIgnoreCase(const TString& src, const TString& endTerm);
	[[nodiscard]] TString PathToName(const TString& path);
	void ForEachToken(const char* str, const std::function<void(std::string_view)> func,
					  const char* separators = " \t\n\r");

	[[nodiscard]] StaticString ToFunctionName(const char* PrettyFunction);
	[[nodiscard]] StaticString ToClassName(const char* PrettyFunction);
	[[nodiscard]] StaticString ToMethodName(const char* PrettyFunction);
	[[nodiscard]] StaticString ToCompactClassName(const char* PrettyFunction);
	[[nodiscard]] StaticString ToCompactMethodName(const char* PrettyFunction);

	[[nodiscard]] size_t StrLen(const char* text);
	[[nodiscard]] size_t StrLen(const char* text, size_t bufferSize);
	[[nodiscard]] const char* StrCopy(char* dst, const char* src, size_t n);
	[[nodiscard]] size_t CalculateHash(const char* text);
	[[nodiscard]] size_t CalculateHash(const std::string_view& str);
}} // namespace hbe::StringUtil

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{

	class StringUtilTest : public TestCollection
	{
	public:
		StringUtilTest() : TestCollection("StringUtilTest") {}

	protected:
		void Prepare() override;
	};

} // namespace hbe
#endif //__UNIT_TEST__
