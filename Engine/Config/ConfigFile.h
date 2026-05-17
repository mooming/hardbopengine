// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <optional>

#include "HSTL/HString.h"
#include "HSTL/HUnorderedMap.h"

namespace hbe
{

	// ConfigFile represent a single config file.
	// Both key and value are string data.
	/// @brief Represents a configuration file with key-value pair parsing.
	class ConfigFile final
	{
	public:
		using TString = HString;
		using TValue = std::optional<TString>;
		using TMap = HUnorderedMap<TString, TString>;

		bool isValid;
		TMap keymap;

	public:
		explicit ConfigFile(const char* path);
		ConfigFile(const char* path, const char* fileName);
		~ConfigFile() = default;

		[[nodiscard]] TValue GetValue(const TString& key) const noexcept;
		[[nodiscard]] TString GetValue(const TString& key, const TString& defaultValue) const noexcept;
		[[nodiscard]] auto IsValid() const noexcept { return isValid; }

		void ForEach(std::function<void(const TMap::value_type&)> func) const noexcept;

	private:
		void Parse(const char* fileName);
	};

} // namespace hbe
