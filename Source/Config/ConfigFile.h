// Created by mooming.go@gmail.com 2016

#pragma once

#include "HSTL/HString.h"
#include "HSTL/HUnorderedMap.h"
#include <optional>


namespace HE
{

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
    
    inline auto IsValid() const { return isValid; }
    
private:
    void Parse(const char* fileName);
};

} //HE
