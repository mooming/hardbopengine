// Created by mooming.go@gmail.com

#pragma once

#include <optional>
#include "HSTL/HString.h"
#include "HSTL/HUnorderedMap.h"

namespace hbe
{

	// ConfigFile represent a single config file.
	// Both key and value are string data.
	class ConfigFile final
	{
	public:
		using TString = hbe::HString;
		using TValue = std::optional<TString>;
		using TMap = hbe::HUnorderedMap<TString, TString>;

		bool isValid;
		TMap keymap;

	public:
		explicit ConfigFile(const char* path);
		ConfigFile(const char* path, const char* fileName);
		~ConfigFile() = default;

		[[nodiscard]] TValue GetValue(const TString& key) const;
		[[nodiscard]] TString GetValue(const TString& key, const TString& defaultValue) const;
		[[nodiscard]] auto IsValid() const { return isValid; }

		void ForEach(std::function<void(const TMap::value_type&)> func) const;

	private:
		void Parse(const char* fileName);
	};

} // namespace hbe
