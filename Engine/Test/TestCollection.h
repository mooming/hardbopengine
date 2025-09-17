// Created by mooming.go@gmail.com, 2017 ~ 2022

#pragma once

#include <cstdint>
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
		using TestFunc = std::function<void(TLogOut& /*ls*/)>;

	private:
		std::string title;
		bool isDone;
		bool isSuccess;

	public:
		struct LogFlush final
		{
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

	protected:
		std::vector<std::pair<std::string, TestFunc>> tests;
		std::vector<std::string> warningMessages;
		std::vector<std::string> errorMessages;

	public:
		explicit TestCollection(const char* title);
		virtual ~TestCollection() = default;

		void Start();
		void AddTest(const char* testName, TestFunc testCase);

		[[nodiscard]]
		auto GetName() const
		{
			return title.c_str();
		}

		[[nodiscard]]
		auto& GetWarningMessages() const
		{
			return warningMessages;
		}

		[[nodiscard]]
		auto& GetErrorMessages() const
		{
			return errorMessages;
		}

		[[nodiscard]]
		bool IsDone() const
		{
			return isDone;
		}

		[[nodiscard]]
		bool IsSuccess() const
		{
			return isSuccess;
		}

	protected:
		friend std::ostream& operator<<(std::ostream& os, const LogFlush& lf);

		virtual void Prepare() = 0;

	private:
		void ExecuteTests();
		void Report() const;
	};

} // namespace hbe
