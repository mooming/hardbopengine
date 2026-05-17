// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "TestCollection.h"


namespace hbe
{

class TestEnv final
{
public:
	TestEnv() noexcept : testedCount(0), passCount(0) {}

	[[nodiscard]] static TestEnv& GetEnv();
	void Start();

	template<typename T, typename... Types>
	void AddTestCollection(Types&&... args)
	{
		tests.push_back(std::make_unique<T>(std::forward(args)...));
	}

private:
	using TCPtr = std::unique_ptr<TestCollection>;

	std::vector<TCPtr> tests;
	std::vector<std::string> invalidTests;
	std::vector<std::string> failedTests;
	std::vector<std::string> warningMessages;
	std::vector<std::string> errorMessages;

	unsigned int testedCount;
	unsigned int passCount;

	bool ExecuteTest(TestCollection& testCollection);
	void Report();
};

} // namespace hbe
