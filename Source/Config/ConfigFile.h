// Created by mooming.go@gmail.com 2016

#pragma once

#include <optional>
#include <string>
#include <unordered_map>


namespace HE
{
    class ConfigFile final
    {
    public:
        using TString = std::string;
        using TValue = std::optional<TString>;

        bool isValid;
        std::unordered_map<TString, TString> keymap;

    public:
        ConfigFile(const char* path);
        ConfigFile(const char* path, const char* fileName);
        ~ConfigFile() = default;

        TValue GetValue(const TString& key) const;
        TString GetValue(const TString& key, const TString& defaultValue) const;

        inline auto IsValid() const { return isValid; }

    private:
        void Parse(const char* fileName);
    };
}
