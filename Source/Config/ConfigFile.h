// Created by mooming.go@gmail.com

#pragma once

#include "HSTL/HString.h"
#include "HSTL/HUnorderedMap.h"
#include <optional>

namespace HE
{

    // ConfigFile represent a single config file.
    // Both key and value are string data.
    class ConfigFile final
    {
    public:
        using TString = HSTL::HString;
        using TValue = std::optional<TString>;
        using TMap = HSTL::HUnorderedMap<TString, TString>;

        bool isValid;
        TMap keymap;

    public:
        ConfigFile(const char* path);
        ConfigFile(const char* path, const char* fileName);
        ~ConfigFile() = default;

        TValue GetValue(const TString& key) const;
        TString GetValue(const TString& key, const TString& defaultValue) const;

        void ForEach(std::function<void(const TMap::value_type&)> func) const;

        inline auto IsValid() const { return isValid; }

    private:
        void Parse(const char* fileName);
    };

} // namespace HE
