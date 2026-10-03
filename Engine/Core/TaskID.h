// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>


namespace hbe
{
/// API reference: docs/Core/TaskID/index.html
struct TaskID final
{
	using TIndex = std::size_t;
	using TGeneration = std::uint32_t;

	static constexpr TIndex NullIndex = std::numeric_limits<TIndex>::max();

	TIndex index = NullIndex;
	TGeneration generation = 0;

	[[nodiscard]] constexpr bool IsNull() const noexcept
	{
		return index == NullIndex;
	}

	friend constexpr bool operator==(const TaskID& lhs, const TaskID& rhs) noexcept = default;
};
} // namespace hbe
