// Created by mooming.go@gmail.com

#pragma once

namespace hbe
{

	/// @brief Log severity levels ordered from least to most severe
	enum class ELogLevel : unsigned char
	{
		Verbose,
		Info,
		Significant,
		Warning,
		Error,
		FatalError,
		MAX
	};

} // namespace hbe
