
// Created by mooming.go@gmail.com, 2022

#pragma once
#include <cstdint>
#include "Intrinsic.h"

namespace hbe
{

	struct source_location
	{
	private:
		const char *fileName;
		const char *functionName;
		const std::uint_least32_t lineNumber;
		const std::uint_least32_t columnOffset;

	public:
		static constexpr source_location current(const char *fileName = CallerFile(),
												 const char *functionName = CallerFunc(),
												 const uint_least32_t lineNumber = CallerLine(),
												 const uint_least32_t columnOffset = CallerColumn()) noexcept
		{
			return source_location{fileName, functionName, lineNumber, columnOffset};
		}

	public:
		constexpr source_location() noexcept :
			fileName("Invalid"), functionName("Invalid"), lineNumber(0), columnOffset(0)
		{}

		source_location(const source_location& rhs) noexcept = default;

		[[nodiscard]] constexpr const char *file_name() const noexcept { return fileName; }

		[[nodiscard]] constexpr const char *function_name() const noexcept { return functionName; }

		[[nodiscard]] constexpr uint_least32_t line() const noexcept { return lineNumber; }

		[[nodiscard]] constexpr std::uint_least32_t column() const noexcept { return columnOffset; }

	private:
		constexpr source_location(const char *fileName, const char *functionName, const uint_least32_t lineNumber,
								  const uint_least32_t columnOffset) noexcept

			: fileName(fileName), functionName(functionName), lineNumber(lineNumber), columnOffset(columnOffset)
		{}
	};

} // namespace hbe
