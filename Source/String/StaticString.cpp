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
#include "Log/Logger.h"
#include <iostream>


namespace HE
{
bool StaticStringTest::DoTest()
{
    using namespace std;
    
    int testCount = 0;
    int failCount = 0;

    auto log = Logger::Get(GetName());

    {
        log.Out([this, testCount](auto& ls)
        {
            ls << "[" << GetName() <<  "][TC" << testCount
                << "] Basic Object Test";
        });

        StaticString str;
    }
    
    ++testCount;
    
    {
        log.Out([testCount](auto& ls)
        {
            ls << "[TC" << testCount
                << "] Basic String Test";
        });

        StaticString str("Hello?");

        log.Out([str](auto& ls)
        {
            ls << str;
        });

        log.Out([str](auto& ls)
        {
            ls << str.c_str();
        });

    }
    
    {
        log.Out([testCount](auto& ls)
        {
            ls << "[TC" << testCount
                << "] Hetero String Test";
        });

        HSTL::HString hello("Hello?");
        HSTL::HInlineString<> helloInline("Hello?");
        
        StaticString str(hello);


        log.Out([str](auto& ls)
        {
            ls << str;
        });

        log.Out([str](auto& ls)
        {
            ls << str.c_str();
        });
        
        StaticString strInline(helloInline);

        log.Out([strInline](auto& ls)
        {
            ls << strInline;
        });


        log.Out([strInline](auto& ls)
        {
            ls << strInline.c_str();
        });
        
        if (str != strInline)
        {
            log.OutError([testCount](auto& ls)
            {
                ls << "[TC" << testCount
                    << "] Test failes due to comparison failure. ";
            });

            ++failCount;
        }
    }
    
    ++testCount;
    
    {
        log.Out([testCount](auto& ls)
        {
            ls << "[TC" << testCount
                << "] Test starts" << endl;
        });

        StaticString str("Hello?");
        
        if (str != str)
        {
            log.OutError([testCount](auto& ls)
            {
                ls << "[TC" << testCount
                    << "] Test failes due to comparison failure.";
            });

            ++failCount;
        }
    }
    
    ++testCount;
    
    {
        log.Out([testCount](auto& ls)
        {
            ls << "[TC" << testCount << "] Test starts";
        });
        
        StaticString strA("Hello?");
        StaticString strB("Hello?");
        
        if (strA != strB)
        {
            log.OutError([testCount](auto& ls)
            {
                ls << "[TC" << testCount
                    << "] Test failes due to comparison failure. ";
            });
            ++failCount;
        }
    }
    
    ++testCount;
    
    {
        log.Out([testCount](auto& ls)
        {
            ls << "[TC" << testCount << "] Test starts";
        });
        
        StaticString strA("Hello?");
        StaticString strB("Ha");
        
        if (strA == strB)
        {
            log.OutError([testCount](auto& ls)
            {
                ls << "[TC" << testCount
                    << "] Test failes due to comparison failure. ";
            });
            
            ++failCount;
        }
    }
    
    return failCount == 0;
}
} // HE

#endif //__UNIT_TEST__

