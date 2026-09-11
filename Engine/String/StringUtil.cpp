// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "StringUtil.h"

#include <algorithm>
#include <memory>
#include "InlineStringBuilder.h"
#include "Log/Logger.h"
#include "Memory/AllocatorScope.h"
#include "Memory/InlinePoolAllocator.h"

#ifdef _MSC_VER
#include <cstdlib>
#include <limits.h>
#include <windows.h>
#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif // PATH_MAX
#endif // _MSC_VER


namespace hbe { namespace StringUtil
{

	TString trim(const TString& str)
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

	TString trimPath(const TString& path)
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

	TString toLowerCase(const TString& src)
	{
		TString result;
		auto ToLowerChar = [](char ch)
		{
			constexpr char diff = 'A' - 'a';

			if ('A' <= ch && ch <= 'Z')
			{
				ch -= diff;
			}

			return ch;
		};

		result.resize(src.size());
		std::transform(src.begin(), src.end(), result.begin(), ToLowerChar);

		return result;
	}

	bool equalsIgnoreCase(const TString& a, const TString& b) { return toLowerCase(a) == toLowerCase(b); }

	bool startsWith(const TString& src, const TString& startTerm)
	{
		if (src.length() < startTerm.length())
		{
			return false;
		}

		TString head = src.substr(0, startTerm.length());

		return head == startTerm;
	}

	bool startsWithIgnoreCase(const TString& src, const TString& startTerm)
	{
		return startsWith(toLowerCase(src), toLowerCase(startTerm));
	}

	bool endsWith(const TString& src, const TString& endTerm)
	{
		if (src.length() < endTerm.length())
		{
			return false;
		}

		const char* srcStr = src.c_str();
		TString last(srcStr + (src.length() - endTerm.length()));

		return last == endTerm;
	}

	bool endsWithIgnoreCase(const TString& src, const TString& endTerm)
	{
		return endsWith(toLowerCase(src), toLowerCase(endTerm));
	}

	TString pathToName(const TString& path)
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

	void forEachToken(const char* str, const std::function<void(std::string_view)> func, const char* separators)
	{
		if (unlikely(str == nullptr))
		{
			return;
		}

		if (unlikely(separators == nullptr))
		{
			func(std::string_view(str));
			return;
		}

		using TIndex = size_t;

		TIndex start = 0;
		TIndex end = 0;

		auto IsSeparator = [separators](char ch) -> bool
		{
			TIndex i = 0;
			char separator = separators[i];

			while (separator != '\0')
			{
				if (ch == separator)
				{
					return true;
				}

				separator = separators[++i];
			}

			return false;
		};

		char ch = str[end];
		while (ch != '\0')
		{
			if (IsSeparator(ch))
			{
				if (start < end)
				{
					const char* interStr = &str[start];
					auto length = end - start;
					func(std::string_view(interStr, length));
				}

				start = end + 1;
			}

			++end;
			ch = str[end];
		}

		if (start < end)
		{
			const char* interStr = &str[start];
			auto length = end - start;
			func(std::string_view(interStr, length));
		}
	}

	StaticString toFunctionName(const char* PrettyFunction)
	{
		using TStr = std::string_view;
		TStr str(PrettyFunction);

		auto bracketEnd = str.find_last_of(')') + 1;
		auto bracketStart = str.find_last_of('(');
		if (unlikely(bracketStart == TStr::npos))
		{
			return StaticString(PrettyFunction);
		}

		auto subStr = str.substr(0, bracketStart);
		auto start = subStr.find_last_of("::") + 1;
		if (start == TStr::npos)
		{
			return StaticString(PrettyFunction);
		}

		str = str.substr(start, bracketEnd - start);

		return StaticString(str);
	}

	StaticString toClassName(const char* PrettyFunction)
	{
		using TStr = std::string_view;
		TStr str(PrettyFunction);

		auto end = str.find_last_of("::") - 1;
		if (end == TStr::npos)
		{
			return StaticString();
		}

		str = str.substr(0, end);

		auto start = str.find_last_of(" ") + 1;
		str = str.substr(start);

		return StaticString(str);
	}

	StaticString toMethodName(const char* PrettyFunction)
	{
		using TStr = std::string_view;
		TStr str(PrettyFunction);

		auto bracketStart = str.find_last_of('(');
		if (bracketStart == TStr::npos)
		{
			return StaticString(PrettyFunction);
		}

		auto subStr = str.substr(0, bracketStart);
		auto start = subStr.find_last_of("::") + 1;
		if (start == TStr::npos)
		{
			return StaticString(PrettyFunction);
		}

		subStr = str.substr(0, start - 2);
		start = subStr.find_last_of(" ") + 1;
		str = str.substr(start, bracketStart - start);

		return StaticString(str);
	}

	StaticString toCompactClassName(const char* PrettyFunction)
	{
		using TStr = std::string_view;
		TStr str(PrettyFunction);

		auto end = str.find_last_of("::") - 1;
		if (end == TStr::npos)
		{
			return StaticString();
		}

		str = str.substr(0, end);
		auto start = str.find_last_of("::") + 1;
		str = str.substr(start, (end - start));

		return StaticString(str);
	}

	StaticString toCompactMethodName(const char* PrettyFunction)
	{
		using TStr = std::string_view;
		TStr str(PrettyFunction);

		auto bracketStart = str.find_last_of('(');
		if (bracketStart == TStr::npos)
		{
			return StaticString(PrettyFunction);
		}

		auto subStr = str.substr(0, bracketStart);
		auto start = subStr.find_last_of("::") + 1;
		if (start == TStr::npos)
		{
			return StaticString(PrettyFunction);
		}

		subStr = str.substr(0, start - 2);
		auto upperStart = subStr.find_last_of("::") + 1;
		if (upperStart == TStr::npos)
		{
			str = str.substr(start);

			return StaticString(str);
		}

		str = str.substr(upperStart, bracketStart - upperStart);

		return StaticString(str);
	}

	size_t strLen(const char* text)
	{
		if (unlikely(text == nullptr))
		{
#ifdef __DEBUG__
			debugBreak();
#endif // __DEBUG__
			return 0;
		}

		return strlen(text);
	}

	size_t strLen(const char* text, size_t n)
	{
		if (unlikely(text == nullptr || n == 0))
		{
#ifdef __DEBUG__
			debugBreak();
#endif // __DEBUG__
			return 0;
		}

		return strnlen(text, n);
	}

	const char* strCopy(char* dst, const char* src, size_t n)
	{
		if (unlikely(dst == nullptr || src == nullptr || dst == src || n == 0))
		{
#ifdef __DEBUG__
			debugBreak();
#endif // __DEBUG__
			return dst;
		}

#ifdef _MSC_VER
		strncpy_s(dst, n, src, n);
		return dst;
#else // _MSC_VER
		return strncpy(dst, src, n);
#endif // _MSC_VER
	}

	size_t calculateHash(const char* text)
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

	size_t calculateHash(const std::string_view& str)
	{
		size_t hashCode = 5381;

		for (size_t ch : str)
		{
			hashCode = ((hashCode << 5) + hashCode) + ch; /* hash * 33 + c */
		}

		return hashCode;
	}

}} // namespace hbe::StringUtil

#ifdef __UNIT_TEST__

namespace hbe
{

	void StringUtilTest::prepare()
	{
		using namespace StringUtil;

		addTest("Tokenizer(default)", [this](auto& ls)
		{
			TVector<TString> tokens;
			auto func = [&tokens](auto token) { tokens.emplace_back(token); };

			forEachToken("abc def    123\n 456  \t\n\r 789    000 end.", func);

			const auto numTokens = tokens.size();
			if (numTokens != 7)
			{
				ls << "Incorrect number of tokens " << tokens.size() << ", 7 is expected." << lferr;
			}

			const char* solutions[7] = {"abc", "def", "123", "456", "789", "000", "end."};

			for (size_t i = 0; i < numTokens; ++i)
			{
				TString solution(solutions[i]);
				if (tokens[i] != solution)
				{
					ls << "Invalid token " << tokens[i].c_str() << ", " << solution.c_str() << " is expected." << lferr;
				}
			}

			for (auto& token : tokens)
			{
				ls << "Token: " << token.c_str() << lf;
			}
		});

		addTest("Tokenizer", [this](auto& ls)
		{
			auto str = "abc::def;;;123.......456::;;..;;::789.000.end.";

			TVector<TString> tokens;
			auto func = [&tokens](auto token) { tokens.emplace_back(token); };

			forEachToken(str, func, ".:;");

			const auto numTokens = tokens.size();
			if (numTokens != 7)
			{
				ls << "Incorrect number of tokens " << tokens.size() << ", 7 is expected." << lferr;
			}

			const char* solutions[7] = {"abc", "def", "123", "456", "789", "000", "end"};

			for (size_t i = 0; i < numTokens; ++i)
			{
				TString solution(solutions[i]);
				if (tokens[i] != solution)
				{
					ls << "Invalid token " << tokens[i].c_str() << ", " << solution.c_str() << " is expected." << lferr;
				}
			}

			for (auto& token : tokens)
			{
				ls << "Token: " << token.c_str() << lf;
			}
		});

		auto prettyFunction = __PRETTY_FUNCTION__;

		addTest("ToClassName", [this, prettyFunction](auto& ls)
		{
			StaticString className("hbe::StringUtilTest");

			auto name = toClassName(prettyFunction);
			ls << "Class Name is " << name << " / " << className << lf;

			if (name != className)
			{
				ls << "ToClassName " << name << " doesn't coincide with " << className << lferr;
			}
		});

		addTest("ToCompactClassName", [this, prettyFunction](auto& ls)
		{
			StaticString className("StringUtilTest");

			auto name = toCompactClassName(prettyFunction);
			ls << "Compact Class Name is " << name << " / " << className << lf;

			if (name != className)
			{
				ls << "ToCompactClassName " << name << " doesn't coincide with " << className << lferr;
			}
		});

		addTest("ToFunctionName::Namespace", [this, prettyFunction](auto& ls)
		{
			StaticString funcName("prepare()");
			StaticString funcName2("prepare(void)");

			auto name = toFunctionName(prettyFunction);
			ls << "Function Name is " << name << " / (" << funcName << " or " << funcName2 << ')' << lf;

			if (name != funcName && name != funcName2)
			{
				ls << "ToFunctionName " << name << " doesn't coincide with niether " << funcName << " nor " << funcName2
				   << lferr;
			}
		});

		addTest("ToMethodName", [this, prettyFunction](auto& ls)
		{
			StaticString funcName("hbe::StringUtilTest::prepare");
			auto name = toMethodName(prettyFunction);

			ls << "Function Name is " << name << " / (" << funcName << ')' << lf;

			if (name != funcName)
			{
				ls << "ToMethodName " << name << " doesn't coincide with niether " << funcName << lferr;
			}
		});

		addTest("ToCompactMethodName", [this, prettyFunction](auto& ls)
		{
			StaticString funcName("StringUtilTest::prepare");
			auto name = toCompactMethodName(prettyFunction);

			ls << "Function Name is " << name << " / (" << funcName << ')' << lf;

			if (name != funcName)
			{
				ls << "ToCompactMethodName " << name << " doesn't coincide with niether " << funcName << lferr;
			}
		});
	}
} // namespace hbe
#endif //__UNIT_TEST__
