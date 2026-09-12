// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <functional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "Log/LogLevel.h"


namespace hbe
{

class TestCollection
{
public:
	using TLogOut = std::stringstream;
	using TLogBuffer = std::vector<std::string>;
	using TTestFunc = std::function<void(TLogOut& /*ls*/)>;

	class LogFlush final
	{
	public:
		const char* name;
		ELogLevel level;
		uint32_t testIndex;
		const char* testName;
		TLogBuffer* messageBuffer;

		LogFlush(const char* name, ELogLevel level, TLogBuffer* buffer);
		~LogFlush() = default;
	};

	LogFlush lf;
	LogFlush lfwarn;
	LogFlush lferr;

	explicit TestCollection(const char* title);
	virtual ~TestCollection() = default;

	void Start();
	void AddTest(const char* testName, const TTestFunc& testCase);

	[[nodiscard]] const char* GetName() const noexcept;
	[[nodiscard]] const std::vector<std::string>& GetWarningMessages() const noexcept;
	[[nodiscard]] const std::vector<std::string>& GetErrorMessages() const noexcept;
	[[nodiscard]] bool IsDone() const noexcept;
	[[nodiscard]] bool IsSuccess() const noexcept;

protected:
	std::vector<std::pair<std::string, TTestFunc>> tests;
	std::vector<std::string> warningMessages;
	std::vector<std::string> errorMessages;

	friend std::ostream& operator<<(std::ostream& os, const LogFlush& lf);

	virtual void Prepare() = 0;

private:
	std::string title;
	bool isDone;
	bool isSuccess;

	void ExecuteTests();
	void Report() const;
};

} // namespace hbe
