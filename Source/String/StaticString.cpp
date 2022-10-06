// Created by mooming.go@gmail.com, 2017

#include "StaticString.h"

#include "StaticStringTable.h"


namespace HE
{

StaticString::StaticString()
{
    static StaticString null("None");
    id = null.id;

#ifdef __DEBUG__
    text[0] = 'N';
    text[1] = 'o';
    text[2] = 'n';
    text[3] = 'e';
    text[4] = '\0';
#endif // __DEBUG__
}

StaticString::StaticString(const char* string)
{
    auto& ssTable = StaticStringTable::GetInstance();
    id = ssTable.Register(string);

#ifdef __DEBUG__
    {
        int i = 0;

        for (; i < 15; ++i)
        {
            char ch = string[i];
            if (ch == '\0')
                break;

            text[i] = ch;
        }
        
        text[i] = '\0';
    }
#endif // __DEBUG__
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
#include "Log/Logger.h"
#include <iostream>


namespace HE
{

void StaticStringTest::Prepare()
{
    AddTest("Default Construct", [](auto&)
    {
        StaticString str;
    });
    
    AddTest("StaticStic Print", [this](TLogOut& ls)
    {
        StaticString str("Hello?");
        ls << str.c_str() << lf;
    });
    
    AddTest("Hetero String Comparison", [this](TLogOut& ls)
    {
        HSTL::HString hello("Hello?");
        HSTL::HInlineString<> helloInline("Hello?");
        
        StaticString str(hello);
        ls << str << lf;
        ls << str.c_str() << lf;
        
        StaticString strInline(helloInline);
        ls << strInline << lf;
        ls << strInline.c_str() << lf;
        
        if (str != strInline)
        {
            ls << "Test failes due to comparison failure." << lferr;
        }
    });
    
    
    AddTest("Self-Comparison", [this](auto& ls)
    {
        StaticString str("Hello?");
        
        if (str != str)
        {
            ls << "Test failes due to comparison failure." << lferr;
        }
    });
    
    AddTest("Two Strings Comparison", [this](auto& ls)
    {
        StaticString strA("Hello?");
        StaticString strB("Hello?");
        
        if (strA != strB)
        {
            ls << "Test failes due to comparison failure." << lferr;
        }
    });
    
    AddTest("Inequality", [this](auto& ls)
    {
        StaticString strA("Hello?");
        StaticString strB("Ha");
        
        if (strA == strB)
        {
            ls << "Test failes due to comparison failure. " << lferr;
        }
    });
}
} // HE

#endif //__UNIT_TEST__

