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
    static_assert(!std::is_signed<TIndex>(), "StaticStringTable::TIndex");
    
    static constexpr size_t NumTables = Config::StaticStringNumHashBuckets;

private:
    template <typename T>
    class Allocator final
    {
    public:
        using value_type = T;

        template <class U>
        struct rebind { using other = Allocator<U>; };

    public:
        Allocator() = default;
        ~Allocator() = default;

        T* allocate(std::size_t n) { return (T*)Allocate(n * sizeof(T)); }
        void deallocate (T*, std::size_t) {}

        template <class U>
        bool operator==(const Allocator<U>& rhs) const { return true; }
        
        template <class U>
        bool operator!=(const Allocator<U>& rhs) const { return false; }
    };

    using TString = std::basic_string<char, std::char_traits<char>, Allocator<char>>;;
    using TTable = std::vector<TString>;
    
private:
    mutable std::mutex tableLock;
    TTable tables[NumTables];
    
public:
    static StaticStringTable& GetInstance();
    
public:
    StaticStringTable();
    ~StaticStringTable();
    
    StaticString GetName() const;

    StaticStringID Register(const char* str);
    StaticStringID Register(const std::string_view& str);
    const char* Get(StaticStringID id) const;
    
    void PrintStringTable() const;
    
private:
    void RegisterPredefinedStrings();
    TIndex GetTableID(const char* text) const;
    TIndex GetTableID(const std::string_view& str) const;

    static void* Allocate(size_t n);
};

} // HE
