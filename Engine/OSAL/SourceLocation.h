
#pragma once

#include <cstdint>
#include "Intrinsic.h"

namespace hbe
{

/// @brief Stores source code location information for debugging and logging.
/// @details Used with macros like CallerFile(), CallerFunc(), CallerLine() to capture
/// the location where a function is called.
class SourceLocation final
{
public:
	static constexpr SourceLocation current(const char* fileName = CallerFile(),
											const char* functionName = CallerFunc(),
											const uint_least32_t lineNumber = CallerLine(),
											const uint_least32_t columnOffset = CallerColumn()) noexcept
	{
		return SourceLocation{fileName, functionName, lineNumber, columnOffset};
	}

	constexpr SourceLocation() noexcept :
		fileName("Invalid"), functionName("Invalid"), lineNumber(0), columnOffset(0)
	{}

	SourceLocation(const SourceLocation& rhs) noexcept = default;

	[[nodiscard]] constexpr const char* file_name() const noexcept { return fileName; }

	[[nodiscard]] constexpr const char* function_name() const noexcept { return functionName; }

	[[nodiscard]] constexpr uint_least32_t line() const noexcept { return lineNumber; }

	[[nodiscard]] constexpr std::uint_least32_t column() const noexcept { return columnOffset; }

private:
	const char* fileName;
	const char* functionName;
	const std::uint_least32_t lineNumber;
	const std::uint_least32_t columnOffset;

	constexpr SourceLocation(const char* fileName, const char* functionName, const uint_least32_t lineNumber,
							 const uint_least32_t columnOffset) noexcept

		: fileName(fileName), functionName(functionName), lineNumber(lineNumber), columnOffset(columnOffset)
	{}
};

} // namespace hbe
