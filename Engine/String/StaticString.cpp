// Created by mooming.go@gmail.com, 2017

#include "StaticString.h"

#include "StaticStringTable.h"

namespace hbe
{

	StaticString::StaticString()
	{
		static StaticString null("None");
		id = null.id;
	}

	StaticString::StaticString(StaticStringID id) : id(id) {}

	StaticString::StaticString(const char* string)
	{
		auto& ssTable = StaticStringTable::GetInstance();
		id = ssTable.Register(string);
	}

	StaticString::StaticString(const std::string_view& str)
	{
		auto& ssTable = StaticStringTable::GetInstance();
		id = ssTable.Register(str);
	}

	const char* StaticString::c_str() const
	{
		auto& ssTable = StaticStringTable::GetInstance();
		return ssTable.Get(id);
	}

} // namespace hbe

#ifdef __UNIT_TEST__

#include <iostream>
#include "HSTL/HString.h"
#include "Log/Logger.h"

namespace hbe
{

	void StaticStringTest::Prepare()
	{
		AddTest("Default Construct", [](auto&) { StaticString str; });

		AddTest("StaticStic Print", [this](TLogOut& ls)
		{
			StaticString str("Hello?");
			ls << str.c_str() << lf;
		});

		AddTest("Hetero String Comparison", [this](TLogOut& ls)
		{
			HSTL::HString hello("Hello?");
			HSTL::HInlineString<> helloInline("Hello?");

			StaticString str(hello);
			ls << str << lf;
			ls << str.c_str() << lf;

			StaticString strInline(helloInline);
			ls << strInline << lf;
			ls << strInline.c_str() << lf;

			if (str != strInline)
			{
				ls << "Test failes due to comparison failure." << lferr;
			}
		});

		AddTest("Self-Comparison", [this](auto& ls)
		{
			StaticString str("Hello?");

			if (str != str)
			{
				ls << "Test failes due to comparison failure." << lferr;
			}
		});

		AddTest("Two Strings Comparison", [this](auto& ls)
		{
			StaticString strA("Hello?");
			StaticString strB("Hello?");

			if (strA != strB)
			{
				ls << "Test failes due to comparison failure." << lferr;
			}
		});

		AddTest("Inequality", [this](auto& ls)
		{
			StaticString strA("Hello?");
			StaticString strB("Ha");

			if (strA == strB)
			{
				ls << "Test failes due to comparison failure. " << lferr;
			}
		});
	}
} // namespace hbe

#endif //__UNIT_TEST__
