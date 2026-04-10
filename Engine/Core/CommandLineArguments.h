// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "Container/Array.h"
#include "String/String.h"

namespace hbe
{
	/// @brief Parses and stores command line arguments passed to the application.
	class CommandLineArguments final
	{
	private:
		String executablePath;
		String executableFilename;
		Array<String> arguments;

	public:
		CommandLineArguments() = delete;
		CommandLineArguments(int argc, const char* argv[]);
		~CommandLineArguments() = default;

		auto& GetArguments() const { return arguments; }

		void Print();

	private:
		void Parse(int argc, const char* argv[]);
	};
} // namespace hbe
