// Created by mooming.go@gmail.com, 2022

#pragma once

namespace hbe
{

	/// @brief A marker struct for inserting newlines in string builders.
	struct EndLine final
	{
		operator const char*() const { return "\n"; }
	};

	static constexpr EndLine hendl;

} // namespace hbe
