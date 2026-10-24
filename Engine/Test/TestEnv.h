// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "TestCollection.h"

namespace hbe
{
/// API reference: docs/Test/TestEnv/index.html
class TestEnv final
{
private:
	using TCPtr = std::unique_ptr<TestCollection>;

	std::vector<TCPtr> tests;
	std::vector<std::size_t> collectionOffsets;
	std::vector<std::string> testletLabels;
	std::vector<std::string> invalidTests;
	std::vector<std::string> failedTests;
	std::vector<std::string> warningMessages;
	std::vector<std::string> errorMessages;

	std::size_t executedTestletCount{0};
	std::vector<unsigned int> testletRunCounts;

	unsigned int testedCount;
	unsigned int passCount;

	bool suiteDrivenByShutdownPump{false};

public:
	[[nodiscard]] static TestEnv& GetEnv();

	TestEnv() noexcept
		: testedCount(0)
		, passCount(0)
	{
	}

	template <typename T, typename... Types>
	void AddTestCollection(Types&&... args)
	{
		tests.push_back(std::make_unique<T>(std::forward(args)...));
	}

	void PrepareTestlets();

	[[nodiscard]] std::size_t GetTestletCount() const noexcept
	{
		return testletLabels.size();
	}

	[[nodiscard]] std::size_t GetExecutedTestletCount() const noexcept
	{
		return executedTestletCount;
	}

	[[nodiscard]] const char* GetTestletLabel(std::size_t testletIndex) const;

	void RunTestlet(std::size_t testletIndex);

	void Finalize();

	[[nodiscard]] std::size_t GetGlobalAllocationBytes() const noexcept;
	[[nodiscard]] std::uint64_t GetGlobalAllocationCount() const noexcept;
	[[nodiscard]] std::size_t GetTestletCountWithGlobalAllocations() const noexcept;

	[[nodiscard]] unsigned int GetPassCount() const noexcept
	{
		return passCount;
	}

	[[nodiscard]] unsigned int GetCollectionCount() const noexcept
	{
		return static_cast<unsigned int>(tests.size());
	}

	[[nodiscard]] unsigned int GetFailureCount() const noexcept
	{
		return static_cast<unsigned int>(failedTests.size() + invalidTests.size()) +
			   (suiteDrivenByShutdownPump ? 1u : 0u);
	}

	void NoteTestletRepeated(std::size_t testletIndex)
	{
		failedTests.push_back("testlet index " + std::to_string(testletIndex) + " ran more than once");
	}

	void NoteSuiteDrivenByShutdownPump() noexcept
	{
		suiteDrivenByShutdownPump = true;
	}

private:
	void FinalizeCollection(TestCollection& testCollection);
	void Report();
};
} // namespace hbe
