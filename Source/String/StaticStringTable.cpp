// Created by mooming.go@gmail.com, 2017

#include "StaticStringTable.h"

#include "Engine.h"
#include "StringUtil.h"
#include "Log/Logger.h"
#include "Memory/AllocatorScope.h"
#include "Config/EngineSettings.h"
#include "System/Debug.h"
#include <iostream>


namespace HE
{

static StaticStringTable* StaticStringTableInstance = nullptr;

StaticStringTable& StaticStringTable::GetInstance()
{
    Assert(StaticStringTableInstance != nullptr);
    return *StaticStringTableInstance;
}

StaticStringTable::StaticStringTable()
    : allocator("StaticStringTable", Config::StaticStringBufferSize)
    
{
    StaticStringTableInstance = this;

    RegisterPredefinedStrings();
}

StaticString StaticStringTable::GetName() const
{
    static StaticString className = StringUtil::PrettyFunctionToCompactClassName(__PRETTY_FUNCTION__);

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
        AllocatorScope scope(allocator.GetID());
        auto& table = tables[tableID];
        auto found = std::find(table.begin(), table.end(), TString(text));
        if (found != table.end())
        {
            id.index = static_cast<TIndex>(std::distance(table.begin(), found));

            return id;
        }

        id.index = static_cast<TIndex>(table.size());
        table.emplace_back(text);
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

} // HE

