// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

namespace hbe
{
/// @brief Identity of a task tracked by the task registry: which record holds it, and which turn that record has
///        had.
/// @details A task used to be identified by where it sat in memory, which meant a work item held a reference into
///          an object anyone could have destroyed. Index plus generation replaces that: the index says which
///          record to look at, and the generation says whether that record is still the one this ID was issued
///          for. A reference to a task that has been released is therefore recognised and dropped rather than
///          followed into whatever task now occupies the record.
/// @details The generation is 32 bits deliberately. A stale ID can only alias a live task after the very same
///          record has been reused four billion times, which is not a window any run reaches; a 16-bit
///          generation makes aliasing an everyday hazard in a table of thousands of records, which would defeat
///          the entire purpose of carrying one.
/// @note Equality compares both halves. Two IDs naming the same record at different generations are different
///       tasks, and an ID that ignores the generation to "just get the task" reintroduces exactly the defect
///       this type exists to close.
/// API reference: docs/Core/TaskID/index.html
struct TaskID final
{
	using TIndex = std::size_t;
	using TGeneration = std::uint32_t;

	/// @brief The index that names no record, so a default-constructed ID refers to nothing.
	static constexpr TIndex NullIndex = std::numeric_limits<TIndex>::max();

	TIndex index = NullIndex;
	TGeneration generation = 0;

	/// @brief Whether this ID names no task at all.
	/// @details Default-constructed IDs and IDs returned by a failed creation are both null, and every operation
	///          that takes an ID accepts null without needing a separate guard at the call site.
	[[nodiscard]] constexpr bool IsNull() const noexcept
	{
		return index == NullIndex;
	}

	friend constexpr bool operator==(const TaskID& lhs, const TaskID& rhs) noexcept = default;
};
} // namespace hbe
