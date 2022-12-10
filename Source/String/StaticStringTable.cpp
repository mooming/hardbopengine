// Created by mooming.go@gmail.com, 2017

#include "StaticStringTable.h"

#include "Engine.h"
#include "StringUtil.h"
#include "Log/Logger.h"
#include "Memory/AllocatorScope.h"
#include "Config/EngineSettings.h"
#include "System/Debug.h"
#include <iostream>
#include <mutex>


namespace HE
{

namespace
{

struct Bank final
{
    size_t cursor;
    size_t capacity;
    uint8_t* buffer;

    Bank(size_t capacity)
        : cursor(0)
        , capacity(capacity)
    {
        buffer = (uint8_t*)malloc(capacity);
    }

    Bank(const Bank& rhs) = delete;

    Bank(Bank&& rhs)
        : cursor(rhs.cursor)
        , capacity(rhs.capacity)
        , buffer(rhs.buffer)
    {
        rhs.cursor = 0;
        rhs.capacity = 0;
        rhs.buffer = nullptr;
    }

    ~Bank()
    {
        if (buffer == nullptr)
            return;

        free(buffer);
    }

    bool IsAvailable(size_t n) const
    {
        return (cursor + n) < capacity;
    }

    uint8_t* Allocate(size_t n)
    {
        auto ptr = &buffer[cursor];
        cursor += n;
        Assert(cursor <= capacity);

        return ptr;
    }
};

std::vector<Bank> banks;
size_t bankIndex = 0;
std::mutex memLock;

} // Anonymous

StaticStringTable& StaticStringTable::GetInstance()
{
    static StaticStringTable instance;
    return instance;
}

StaticStringTable::StaticStringTable()
{
    RegisterPredefinedStrings();
}

StaticStringTable::~StaticStringTable()
{
    banks.clear();
    banks.shrink_to_fit();
}

StaticString StaticStringTable::GetName() const
{
    static StaticString className = StringUtil::ToCompactClassName(__PRETTY_FUNCTION__);

    return className;
}

StaticStringID StaticStringTable::Register(const char* text)
{
    StaticStringID id;

    auto tableID = GetTableID(text);
    id.tableID = tableID;
    
    static_assert(!std::is_signed<decltype(tableID)>());
    Assert(tableID < NumTables);

    {
        std::lock_guard lock(tableLock);

        TString str(text);

        auto& table = tables[tableID];
        auto found = std::find(table.begin(), table.end(), str);
        if (found != table.end())
        {
            id.index = static_cast<TIndex>(std::distance(table.begin(), found));

            return id;
        }

        id.index = static_cast<TIndex>(table.size());
        table.emplace_back(std::move(str));
    }
    
    return id;
}

StaticStringID StaticStringTable::Register(const std::string_view& str)
{
    StaticStringID id;

    auto tableID = GetTableID(str);
    id.tableID = tableID;

    static_assert(!std::is_signed<decltype(tableID)>());
    Assert(tableID < NumTables);

    {
        std::lock_guard lock(tableLock);

        auto& table = tables[tableID];
        auto found = std::find(table.begin(), table.end(), str);
        if (found != table.end())
        {
            id.index = static_cast<TIndex>(std::distance(table.begin(), found));

            return id;
        }

        id.index = static_cast<TIndex>(table.size());
        
        TString tempStr(str.begin(), str.end());
        table.emplace_back(std::move(tempStr));
    }

    return id;
}

const char* StaticStringTable::Get(StaticStringID id) const
{
    const char* str = "None";

    static_assert(!std::is_signed<decltype(id.tableID)>());
    if (unlikely(id.tableID >= NumTables))
        return str;

    static_assert(!std::is_signed<decltype(id.index)>());

    {
        std::lock_guard lock(tableLock);
        auto& table = tables[id.tableID];

        if (unlikely(id.index >= table.size()))
            return "None";

        str = table[id.index].c_str();
    }

    return str;
}

void StaticStringTable::PrintStringTable() const
{
    auto log = Logger::Get(GetName(), ELogLevel::Verbose);
    
    log.Out("= StringTable ==============================");
    log.Out([](auto& ls)
    {
        ls << "Number of Banks = " << banks.size();
    });

    {
        size_t index = 0;
        for (auto& bank : banks)
        {
            log.Out([index, &bank](auto& ls)
            {
                ls << index << " : " << bank.cursor << " / " << bank.capacity;
            });

            ++index;
        }
    }

    TIndex tableID = 0;
    
    for (auto& table : tables)
    {
        if (table.size() <= 0)
            continue;
        
        log.Out([&](auto& ls)
        {
            ls << "Table ID = " << tableID << "/" << NumTables
                << ", Number of Elements = "
                << table.size();
        });
        
        ++tableID;
    }
    
    size_t count = 0;
    tableID = 0;

    for (auto& table : tables)
    {
        TIndex index = 0;
        
        for (auto& item : table)
        {
            StaticStringID id;
            id.tableID = tableID;
            id.index = index;
            
            log.Out([&](auto& ls)
            {
                ls << count++ << " : [" << item << "] "
                    << id.value << '(' << id.tableID << ','
                    << id.index << ')';
            });
            
            ++index;
        }
        
        ++tableID;
    }
    
    log.Out([count](auto& ls)
    {
        ls << "Number of elements = " << count;
    });

    log.Out("============================================\n");
}

void StaticStringTable::RegisterPredefinedStrings()
{
    Register("");
    Register("None");
    Register("NONE");
    Register("Null");
    Register("NULL");
    Register("True");
    Register("False");
    Register("Unknown");

    Register("HardbopEngine");
}

StaticStringTable::TIndex StaticStringTable::GetTableID(const char* text) const
{
    auto hashValue = StringUtil::CalculateHash(text);
    auto tableId = static_cast<TIndex>(hashValue % NumTables);
    
    return tableId;
}

StaticStringTable::TIndex StaticStringTable::GetTableID(const std::string_view& str) const
{
    auto hashValue = StringUtil::CalculateHash(str);
    auto tableId = static_cast<TIndex>(hashValue % NumTables);

    return tableId;
}

void* StaticStringTable::Allocate(size_t n)
{
    constexpr size_t capacity = 1024UL * 1024;

    std::lock_guard lock(memLock);
    while (banks.size() <= bankIndex)
    {
        banks.emplace_back(capacity);
    }

    if (!banks[bankIndex].IsAvailable(n))
    {
        banks.emplace_back(std::max(n, capacity));
        ++bankIndex;
    }

    auto& bank = banks[bankIndex];
    auto ptr = bank.Allocate(n);

    return ptr;
}

} // HE
