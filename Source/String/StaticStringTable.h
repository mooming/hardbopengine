// Created by mooming.go@gmail.com, 2022

#pragma once

#include "StaticString.h"
#include "StaticStringID.h"
#include "HSTL/HString.h"
#include "HSTL/HVector.h"
#include "HSTL/HUnorderedMap.h"
#include "HSTL/HVector.h"
#include "Memory/StackAllocator.h"
#include <cstddef>


namespace HE
{

class StaticStringTable final
{
public:
    using TIndex = StaticStringID::TIndex;
    using TString = HSTL::HString;
    using TTable = HSTL::HVector<TString>;
    static_assert(!std::is_signed<TIndex>(), "StaticStringTable::TIndex");
    
    static constexpr size_t NumTables = Config::StaticStringNumHashBuckets;
    
private:
    StackAllocator allocator;
    TTable tables[NumTables];
    
public:
    static StaticStringTable& GetInstance();
    
public:
    StaticStringTable();
    ~StaticStringTable() = default;
    
    StaticString GetName() const;
    
    StaticStringID Register(const char* str);
    const char* Get(StaticStringID id) const;
    
    void PrintStringTable() const;
    
private:
    void RegisterPredefinedStrings();
    size_t GetTableID(const char* text) const;
};

} // HE
