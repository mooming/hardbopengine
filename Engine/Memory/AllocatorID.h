// Created by mooming.go@gmail.com

#pragma once

namespace hbe
{

	/// @brief Type alias for allocator identifier
	using TAllocatorID = int;

	/// @brief Maximum number of allocators supported in the system
	static constexpr TAllocatorID MaxNumAllocators = 256;

	/// @brief Sentinel value representing an invalid/uninitialized allocator
	static constexpr TAllocatorID InvalidAllocatorID = -1;

	/// @brief Check if an allocator ID is valid (within range)
	inline bool IsValid(TAllocatorID id) { return id >= 0 && id < MaxNumAllocators; }

} // namespace hbe
