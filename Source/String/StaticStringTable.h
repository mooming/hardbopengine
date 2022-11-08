// Created by mooming.go@gmail.com, 2022

#pragma once

#include "StaticString.h"
#include "StaticStringID.h"
#include "Config/EngineSettings.h"
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string_view>


namespace HE
{

class StaticStringTable final
{
public:
    using TIndex = StaticStringID::TIndex;
    using TString = std::string;
    using TTable = std::vector<TString>;
    static_assert(!std::is_signed<TIndex>(), "StaticStringTable::TIndex");
    
    static constexpr size_t NumTables = Config::StaticStringNumHashBuckets;
    
private:
    mutable std::mutex tableLock;
    TTable tables[NumTables];
    
public:
    static StaticStringTable& GetInstance();
    
public:
    StaticStringTable();
    ~StaticStringTable() = default;
    
    StaticString GetName() const;

    StaticStringID Register(const char* str);
    StaticStringID Register(const std::string_view& str);
    const char* Get(StaticStringID id) const;
    
    void PrintStringTable() const;
    
private:
    void RegisterPredefinedStrings();
    TIndex GetTableID(const char* text) const;
    TIndex GetTableID(const std::string_view& str) const;
};

} // HE
