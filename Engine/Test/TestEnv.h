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

	[[nodiscard]] static TestEnv& getEnv();
	void start();

	/// @brief Collections that completed successfully.
	[[nodiscard]] unsigned int getPassCount() const noexcept { return passCount; }

	/// @brief Collections that ran and failed, plus any that never completed.
	/// @details Valid after start(); start() clears both lists on entry.
	[[nodiscard]] unsigned int getFailureCount() const noexcept
	{
		return static_cast<unsigned int>(failedTests.size() + invalidTests.size());
	}

	template<typename T, typename... Types>
	void addTestCollection(Types&&... args)
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

	bool executeTest(TestCollection& testCollection);
	void report();
};

} // namespace hbe
