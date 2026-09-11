// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "InlineStringBuilder.h"


#ifdef __UNIT_TEST__
#include <limits>
#include "HSTL/HString.h"

namespace hbe
{

	void InlineStringBuilderTest::prepare()
	{
		constexpr size_t MaxLength = 1024;
		using TString = hbe::HInlineString<MaxLength>;
		using TStrBuild = InlineStringBuilder<MaxLength>;

		addTest("Default Construct", [this](auto& ls)
		{
			TStrBuild strBuild;
			auto str = strBuild.c_str();

			if (str[0] != '\0')
			{
				ls << "Default constructed StringBuild doesn't provide an "
					  "empty string."
				   << lferr;
			}

			strBuild.clear();
			str = strBuild.c_str();

			if (str[0] != '\0')
			{
				ls << "Clear() varies the internal string." << lferr;
			}
		});

		addTest("Add Nullptr", [this](auto& ls)
		{
			TStrBuild strBuild;
			strBuild << nullptr;

			TString str(strBuild.c_str());
			TString expected("Null");

			ls << "Result: " << str << " <=> " << expected << lf;

			if (str != expected)
			{
				ls << "Invalid result " << str << ", but " << expected << " expected." << lferr;
			}
		});

		addTest("Add Boolean(true)", [this](auto& ls)
		{
			TStrBuild strBuild;
			strBuild << true;

			TString str(strBuild.c_str());
			TString expected("True");

			ls << "Result: " << str << " <=> " << expected << lf;

			if (str != expected)
			{
				ls << "Invalid result " << str << ", but " << expected << " expected." << lferr;
			}
		});

		addTest("Add Boolean(false)", [this](auto& ls)
		{
			TStrBuild strBuild;
			strBuild << false;

			TString str(strBuild.c_str());
			TString expected("False");

			ls << "Result: " << str << " <=> " << expected << lf;

			if (str != expected)
			{
				ls << "Invalid result " << str << ", but " << expected << " expected." << lferr;
			}
		});

		addTest("Add Boolean(true, false)", [this](auto& ls)
		{
			TStrBuild strBuild;
			strBuild << true << ", " << false;

			TString str(strBuild.c_str());
			TString expected("True, False");

			ls << "Result: " << str << " <=> " << expected << lf;

			if (str != expected)
			{
				ls << "Invalid result " << str << ", but " << expected << " expected." << lferr;
			}
		});

		addTest("Add Char", [this](auto& ls)
		{
			TStrBuild strBuild;
			strBuild << 'a';

			TString str(strBuild.c_str());
			TString expected("a");

			ls << "Result: " << str << " <=> " << expected << lf;

			if (str != expected)
			{
				ls << "Invalid result " << str << ", but " << expected << " expected." << lferr;
			}
		});

		addTest("Add Chars", [this](auto& ls)
		{
			TStrBuild strBuild;
			strBuild << 'a' << 'b' << 'c';

			TString str(strBuild.c_str());
			TString expected("abc");

			ls << "Result: " << str << " <=> " << expected << lf;

			if (str != expected)
			{
				ls << "Invalid result " << str << ", but " << expected << " expected." << lferr;
			}
		});

		addTest("uint8_t", [this](auto& ls)
		{
			TStrBuild strBuild;

			using T = uint8_t;
			T value = std::numeric_limits<T>::max();
			strBuild << value;

			TString str(strBuild.c_str());
			TString expected(std::to_string(value));

			ls << "Result: " << str << " <=> " << expected << lf;

			if (str != expected)
			{
				ls << "Invalid result " << str << ", but " << expected << " expected." << lferr;
			}
		});

		addTest("int16_t", [this](auto& ls)
		{
			TStrBuild strBuild;

			using T = int16_t;
			T value = std::numeric_limits<T>::max();
			strBuild << value;

			TString str(strBuild.c_str());
			TString expected(std::to_string(value));

			ls << "Result: " << str << " <=> " << expected << lf;

			if (str != expected)
			{
				ls << "Invalid result " << str << ", but " << expected << " expected." << lferr;
			}
		});

		addTest("uint16_t", [this](auto& ls)
		{
			TStrBuild strBuild;

			using T = uint16_t;
			T value = std::numeric_limits<T>::max();
			strBuild << value;

			TString str(strBuild.c_str());
			TString expected(std::to_string(value));

			ls << "Result: " << str << " <=> " << expected << lf;

			if (str != expected)
			{
				ls << "Invalid result " << str << ", but " << expected << " expected." << lferr;
			}
		});

		addTest("int32_t", [this](auto& ls)
		{
			TStrBuild strBuild;

			using T = int32_t;
			T value = std::numeric_limits<T>::max();
			strBuild << value;

			TString str(strBuild.c_str());
			TString expected(std::to_string(value));

			ls << "Result: " << str << " <=> " << expected << lf;

			if (str != expected)
			{
				ls << "Invalid result " << str << ", but " << expected << " expected." << lferr;
			}
		});

		addTest("uint32_t", [this](auto& ls)
		{
			TStrBuild strBuild;

			using T = uint32_t;
			T value = std::numeric_limits<T>::max();
			strBuild << value;

			TString str(strBuild.c_str());
			TString expected(std::to_string(value));

			ls << "Result: " << str << " <=> " << expected << lf;

			if (str != expected)
			{
				ls << "Invalid result " << str << ", but " << expected << " expected." << lferr;
			}
		});

		addTest("int64_t", [this](auto& ls)
		{
			TStrBuild strBuild;

			using T = int64_t;
			T value = std::numeric_limits<T>::max();
			strBuild << value;

			TString str(strBuild.c_str());
			TString expected(std::to_string(value));

			ls << "Result: " << str << " <=> " << expected << lf;

			if (str != expected)
			{
				ls << "Invalid result " << str << ", but " << expected << " expected." << lferr;
			}
		});

		addTest("uint64_t", [this](auto& ls)
		{
			TStrBuild strBuild;

			using T = uint64_t;
			T value = std::numeric_limits<T>::max();
			strBuild << value;

			TString str(strBuild.c_str());
			TString expected(std::to_string(value));

			ls << "Result: " << str << " <=> " << expected << lf;

			if (str != expected)
			{
				ls << "Invalid result " << str << ", but " << expected << " expected." << lferr;
			}
		});

		addTest("float", [this](auto& ls)
		{
			TStrBuild strBuild;

			using T = float;
			T value = std::numeric_limits<T>::max();
			strBuild << value;

			TString str(strBuild.c_str());
			TString expected(std::to_string(value));

			ls << "Result: " << str << " <=> " << expected << lf;

			if (str != expected)
			{
				ls << "Invalid result " << str << ", but " << expected << " expected." << lferr;
			}
		});

		addTest("double", [this](auto& ls)
		{
			TStrBuild strBuild;

			using T = double;
			T value = std::numeric_limits<T>::max();
			strBuild << value;

			TString str(strBuild.c_str());
			TString expected(std::to_string(value));

			ls << "Result: " << str << " <=> " << expected << lf;

			if (str != expected)
			{
				ls << "Invalid result " << str << ", but " << expected << " expected." << lferr;
			}
		});

		addTest("long double", [this](auto& ls)
		{
			TStrBuild strBuild;

			using T = long double;
			T value = std::numeric_limits<T>::max();
			strBuild << value;

			TString str(strBuild.c_str());

			char temp[1024];
			snprintf(temp, sizeof(temp), "%Le", value);
			TString expected(temp);

			ls << "Result: " << str << " <=> " << expected << lf;

			if (str != expected)
			{
				ls << "Invalid result " << str << ", but " << expected << " expected." << lferr;
			}
		});
	}

} // namespace hbe
#endif //__UNIT_TEST__
