// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "HInlineFunction.h"

#ifdef TEST_ENABLED

void hbe::HInlineFunctionTest::Prepare()
{
	AddTest("Invoke With Captures", [this](auto& ls)
	{
		HInlineFunction<int(int, int), 64> add = [base = 10](int a, int b) { return a + b + base; };

		const int result = add(1, 2);
		ls << "add(1, 2) with base 10 = " << result << lf;

		if (result != 13)
		{
			ls << "Unexpected result" << lferr;

			return;
		}

		ls << "Invoke passed" << lf;
	});

	AddTest("Void Signature With Reference Argument", [this](auto& ls)
	{
		HInlineFunction<void(int&), 64> doubler = [](int& value) { value *= 2; };

		int value = 21;
		doubler(value);
		ls << "doubled value = " << value << lf;

		if (value != 42)
		{
			ls << "The reference argument did not reach the callable" << lferr;

			return;
		}

		ls << "Reference argument passed" << lf;
	});

	AddTest("Move Constructor Transfers The Callable", [this](auto& ls)
	{
		HInlineFunction<int(), 64> source = []() { return 7; };

		HInlineFunction<int(), 64> target = std::move(source);
		const int result = target();
		ls << "target() = " << result << ", source valid = " << (source.IsValid() ? "true" : "false") << lf;

		if (source.IsValid() || result != 7)
		{
			ls << "The callable was not transferred by the move constructor" << lferr;

			return;
		}

		ls << "Move constructor passed" << lf;
	});

	AddTest("Move Assignment Transfers The Callable", [this](auto& ls)
	{
		HInlineFunction<int(), 64> first = []() { return 1; };

		HInlineFunction<int(), 64> second = []() { return 2; };

		second = std::move(first);
		const int result = second();
		ls << "second() = " << result << ", first valid = " << (first.IsValid() ? "true" : "false") << lf;

		if (first.IsValid() || result != 1)
		{
			ls << "The callable was not transferred by the move assignment" << lferr;

			return;
		}

		ls << "Move assignment passed" << lf;
	});

	AddTest("Default Constructed Function Is Invalid", [this](auto& ls)
	{
		HInlineFunction<int(), 64> empty;
		ls << "empty valid = " << (empty.IsValid() ? "true" : "false") << lf;

		if (empty.IsValid())
		{
			ls << "A default constructed function should be invalid" << lferr;

			return;
		}

		ls << "Empty state passed" << lf;
	});
}
#endif // TEST_ENABLED
