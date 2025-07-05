// Created by mooming.go@gmail.com, 2022

#include "StringBuilder.h"

namespace HE
{
	template class StringBuilder<char, BaseAllocator<char>>;
} // namespace HE

#ifdef __UNIT_TEST__
#include <limits>
#include "HSTL/HString.h"

namespace HE
{

	void StringBuilderTest::Prepare()
	{
		using TString = HSTL::HInlineString<256>;
		using TStrBuild = StringBuilder<>;

		AddTest("Default Construct", [this](auto& ls)
		{
			TStrBuild strBuild;
			auto str = strBuild.c_str();

			if (str[0] != '\0')
			{
				ls << "Default constructed StringBuild doesn't provide an "
					  "empty string."
				   << lferr;
			}

			strBuild.Reserve(100);
			str = strBuild.c_str();

			if (str[0] != '\0')
			{
				ls << "Reserve() varies the internal string." << lferr;
			}

			strBuild.Clear();
			str = strBuild.c_str();

			if (str[0] != '\0')
			{
				ls << "Clear() varies the internal string." << lferr;
			}
		});

		AddTest("Add Nullptr", [this](auto& ls)
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

		AddTest("Add Boolean(true)", [this](auto& ls)
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

		AddTest("Add Boolean(false)", [this](auto& ls)
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

		AddTest("Add Boolean(true, false)", [this](auto& ls)
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

		AddTest("Add Char", [this](auto& ls)
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

		AddTest("Add Chars", [this](auto& ls)
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

		AddTest("uint8_t", [this](auto& ls)
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

		AddTest("int16_t", [this](auto& ls)
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

		AddTest("uint16_t", [this](auto& ls)
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

		AddTest("int32_t", [this](auto& ls)
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

		AddTest("uint32_t", [this](auto& ls)
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

		AddTest("int64_t", [this](auto& ls)
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

		AddTest("uint64_t", [this](auto& ls)
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

		AddTest("float", [this](auto& ls)
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

		AddTest("double", [this](auto& ls)
		{
			TStrBuild strBuild;

			using T = double;
			T value = std::numeric_limits<T>::max();
			strBuild << value;

			TString str(strBuild.c_str());

			char temp[1024];
			snprintf(temp, sizeof(temp), "%lf", value);
			TString expected(temp);

			ls << "Result: " << str << " <=> " << expected << lf;

			if (str != expected)
			{
				ls << "Invalid result " << str << ", but " << expected << " expected." << lferr;
			}
		});

		AddTest("long double", [this](auto& ls)
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

} // namespace HE
#endif //__UNIT_TEST__
