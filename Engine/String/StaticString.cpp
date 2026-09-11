// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "StaticString.h"

#include "StaticStringTable.h"


namespace hbe
{

	StaticString::StaticString() noexcept
	{
		static StaticString null("None");
		id = null.id;
	}

	StaticString::StaticString(StaticStringID id) noexcept : id(id) {}

	StaticString::StaticString(const char* string) noexcept
	{
		auto& ssTable = StaticStringTable::getInstance();
		id = ssTable.Register(string);
	}

	StaticString::StaticString(const std::string_view& str) noexcept
	{
		auto& ssTable = StaticStringTable::getInstance();
		id = ssTable.Register(str);
	}

	const char* StaticString::c_str() const noexcept
	{
		auto& ssTable = StaticStringTable::getInstance();
		return ssTable.get(id);
	}

} // namespace hbe

#ifdef __UNIT_TEST__

#include <iostream>
#include "HSTL/HString.h"
#include "Log/Logger.h"

namespace hbe
{

	void StaticStringTest::prepare()
	{
		addTest("Default Construct", [](auto&) { StaticString str; });

		addTest("StaticStic Print", [this](TLogOut& ls)
		{
			StaticString str("Hello?");
			ls << str.c_str() << lf;
		});

		addTest("Hetero String Comparison", [this](TLogOut& ls)
		{
			hbe::HString hello("Hello?");
			hbe::HInlineString<> helloInline("Hello?");

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

		addTest("Self-Comparison", [this](auto& ls)
		{
			StaticString str("Hello?");

			if (str != str)
			{
				ls << "Test failes due to comparison failure." << lferr;
			}
		});

		addTest("Two Strings Comparison", [this](auto& ls)
		{
			StaticString strA("Hello?");
			StaticString strB("Hello?");

			if (strA != strB)
			{
				ls << "Test failes due to comparison failure." << lferr;
			}
		});

		addTest("Inequality", [this](auto& ls)
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
