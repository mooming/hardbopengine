// Created by mooming.go@gmail.com, 2017

#include "StaticStringTable.h"

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
    static auto className = StringUtil::PrettyFunctionToCompactClassName(__PRETTY_FUNCTION__);
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
        id.index = std::distance(table.begin(), found);
        
        return id;
    }
    
    TString str(text);
    id.index = table.size();
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
    using namespace std;
    
    cout << "[" << GetName() << "] PrintStringTable ===" << endl;
    
    size_t tableID = 0;
    
    for (auto& table : tables)
    {
        if (table.size() <= 0)
            continue;
        
        cout << "## Table ID = " << tableID << "/" << NumTables
            << ", Number of Elements = "
            << table.size() << endl;
        
        ++tableID;
    }
    
    size_t count = 0;
    
    tableID = 0;

    for (auto& table : tables)
    {
        size_t index = 0;
        
        for (auto& item : table)
        {
            StaticStringID id;
            id.tableID = tableID;
            id.index = index;
            
            cout << count++ << " : [" << item << "] "
                << id.value << '(' << id.tableID << ','
                << id.index << ')' << endl;
            ++index;
        }
        
        ++tableID;
    }
    
    cout << "[" << GetName() << "] Number of elements = " << count << endl;
}

void StaticStringTable::RegisterPredefinedStrings()
{
    Register("");
    Register("None");
    Register("NONE");
    Register("Null");
    Register("NULL");
    Register("HardbopEngine");
}

size_t StaticStringTable::GetTableID(const char* text) const
{
    auto hashValue = StringUtil::CalculateHash(text);
    auto tableId = hashValue % NumTables;
    
    return tableId;
}

} // HE

