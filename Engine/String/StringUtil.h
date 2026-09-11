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

	[[nodiscard]] TString trim(const TString& str);
	[[nodiscard]] TString trimPath(const TString& path);
	[[nodiscard]] TString toLowerCase(const TString& src);
	[[nodiscard]] bool equalsIgnoreCase(const TString& a, const TString& b);
	[[nodiscard]] bool startsWith(const TString& src, const TString& startTerm);
	[[nodiscard]] bool startsWithIgnoreCase(const TString& src, const TString& startTerm);
	[[nodiscard]] bool endsWith(const TString& src, const TString& endTerm);
	[[nodiscard]] bool endsWithIgnoreCase(const TString& src, const TString& endTerm);
	[[nodiscard]] TString pathToName(const TString& path);
	void forEachToken(const char* str, const std::function<void(std::string_view)> func,
					  const char* separators = " \t\n\r");

	[[nodiscard]] StaticString toFunctionName(const char* PrettyFunction);
	[[nodiscard]] StaticString toClassName(const char* PrettyFunction);
	[[nodiscard]] StaticString toMethodName(const char* PrettyFunction);
	[[nodiscard]] StaticString toCompactClassName(const char* PrettyFunction);
	[[nodiscard]] StaticString toCompactMethodName(const char* PrettyFunction);

	[[nodiscard]] size_t strLen(const char* text);
	[[nodiscard]] size_t strLen(const char* text, size_t bufferSize);
	[[nodiscard]] const char* strCopy(char* dst, const char* src, size_t n);
	[[nodiscard]] size_t calculateHash(const char* text);
	[[nodiscard]] size_t calculateHash(const std::string_view& str);
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
		void prepare() override;
	};

} // namespace hbe
#endif //__UNIT_TEST__
