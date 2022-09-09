// Created by mooming.go@gmail.com, 2017

#include "StaticStringTable.h"

#include "Engine.h"
#include "StringUtil.h"
#include "Config/EngineConfig.h"
#include "Memory/AllocatorScope.h"
#include "OSAL/Intrinsic.h"
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
    AllocatorScope scope(allocator.GetID());
    
    StaticStringID id;
    
    auto tableID = GetTableID(text);
    id.tableID = tableID;
    
    static_assert(!std::is_signed<decltype(tableID)>());
    Assert(tableID < NumTables);
    
    auto& table = tables[tableID];
    auto found = std::find(table.begin(), table.end(), TString(text));
    if (found != table.end())
    {
        id.index = static_cast<TIndex>(std::distance(table.begin(), found));
        
        return id;
    }
    
    TString str(text);
    id.index = static_cast<TIndex>(table.size());
    table.push_back(str);
    
    return id;
}

const char* StaticStringTable::Get(StaticStringID id) const
{
    static_assert(!std::is_signed<decltype(id.tableID)>());
    if (unlikely(id.tableID >= NumTables))
        return "None";
    
    auto& table = tables[id.tableID];
    static_assert(!std::is_signed<decltype(id.index)>());
    if (unlikely(id.index >= table.size()))
        return "None";
    
    return table[id.index].c_str();
}

void StaticStringTable::PrintStringTable() const
{
    auto& engine = Engine::Get();
    engine.Log(ELogLevel::Verbose, [](auto& ls)
    {
        ls << "StringTable ============";
    });
    
    TIndex tableID = 0;
    
    for (auto& table : tables)
    {
        if (table.size() <= 0)
            continue;
        
        engine.Log(ELogLevel::Verbose, [&](auto& ls)
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
            
            engine.Log(ELogLevel::Verbose, [&](auto& ls)
            {
                ls << count++ << " : [" << item << "] "
                    << id.value << '(' << id.tableID << ','
                    << id.index << ')';
            });
            
            ++index;
        }
        
        ++tableID;
    }
    
    engine.Log(ELogLevel::Verbose, [count](auto& ls)
    {
        ls << "Number of elements = " << count;
    });
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

    Register("HardbopEngine");
}

StaticStringTable::TIndex StaticStringTable::GetTableID(const char* text) const
{
    auto hashValue = StringUtil::CalculateHash(text);
    auto tableId = static_cast<TIndex>(hashValue % NumTables);
    
    return tableId;
}

} // HE

