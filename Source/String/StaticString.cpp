// Created by mooming.go@gmail.com, 2017

#include "StaticString.h"

#include "StaticStringTable.h"


namespace HE
{

StaticString::StaticString()
{
    static StaticString null("None");
    id = null.id;
}

StaticString::StaticString(const char* string)
{
    auto& ssTable = StaticStringTable::GetInstance();
    id = ssTable.Register(string);
}

const char* StaticString::c_str() const
{
    auto& ssTable = StaticStringTable::GetInstance();
    auto str = ssTable.Get(id);
    
    return str;
}

} // HE


#ifdef __UNIT_TEST__

#include "HSTL/HString.h"
#include <iostream>


namespace HE
{
bool StaticStringTest::DoTest()
{
    using namespace std;
    
    int testCount = 0;
    int failCount = 0;
    
    {
        cout << "[" << GetName() <<  "][TC" << testCount
            << "] Basic Object Test" << endl;
        StaticString str;
    }
    
    ++testCount;
    
    {
        cout << "[" << GetName() <<  "][TC" << testCount
            << "] Basic String Test" << endl;
        
        StaticString str("Hello?");
        cout << str << endl;
        cout << str.c_str() << endl;
    }
    
    {
        cout << "[" << GetName() <<  "][TC" << testCount
            << "] Hetero String Test" << endl;
        
        HSTL::HString hello("Hello?");
        HSTL::HInlineString<> helloInline("Hello?");
        
        StaticString str(hello);
        cout << str << endl;
        cout << str.c_str() << endl;
        
        StaticString strInline(helloInline);
        cout << strInline << endl;
        cout << strInline.c_str() << endl;
        
        if (str != strInline)
        {
            cerr << "[" << GetName() <<  "][Error][TC" << testCount
                << "] Test failes due to comparison failure. " << endl;
            ++failCount;
        }
    }
    
    ++testCount;
    
    {
        cout << "[" << GetName() <<  "][TC" << testCount
            << "] Test starts" << endl;
        
        StaticString str("Hello?");
        
        if (str != str)
        {
            cerr << "[" << GetName() <<  "][Error][TC" << testCount
                << "] Test failes due to comparison failure. " << endl;
            ++failCount;
        }
    }
    
    ++testCount;
    
    {
        cout << "[" << GetName() <<  "][TC" << testCount
            << "] Test starts" << endl;
        
        StaticString strA("Hello?");
        StaticString strB("Hello?");
        
        if (strA != strB)
        {
            cerr << "[" << GetName() <<  "][Error][TC" << testCount
                << "] Test failes due to comparison failure. " << endl;
            ++failCount;
        }
    }
    
    ++testCount;
    
    {
        cout << "[" << GetName() <<  "][TC" << testCount
            << "] Test starts" << endl;
        
        StaticString strA("Hello?");
        StaticString strB("Ha");
        
        if (strA == strB)
        {
            cerr << "[" << GetName() <<  "][Error][TC" << testCount
                << "] Test failes due to comparison failure. " << endl;
            ++failCount;
        }
    }
    
    return failCount == 0;
}
} // HE

#endif //__UNIT_TEST__

