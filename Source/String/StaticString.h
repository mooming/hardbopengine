// Created by mooming.go@gmail.com, 2017

#pragma once

#include "StaticStringID.h"
#include "Config/BuildConfig.h"
#include <ostream>


namespace HE
{

class StaticString final
{
private:
    StaticStringID id;

#ifdef __DEBUG__
    char text[16];
#endif // __DEBUG__
    
public:
    StaticString();
    inline StaticString(StaticStringID id)
        : id (id)
    {
#ifdef __DEBUG__
        text[0] = '\0';
#endif // __DEBUG__
    }
    
    StaticString(const char* string);
    
    template <typename T>
    StaticString(const T& string)
        : StaticString(string.c_str())
    {
    }
    
    ~StaticString() = default;
    
    const char* c_str() const;
    
    inline auto GetID() const noexcept { return id; }
    inline bool IsNull() const noexcept { return id.value == 0; }
    inline operator const char* () const { return c_str(); }
    inline bool operator < (const StaticString& rhs) const { return id.value < rhs.id.value; }
    inline bool operator == (const StaticString& rhs) const { return id.value == rhs.id.value; }
    
    inline friend std::ostream& operator <<(std::ostream& os, const StaticString& str)
    {
        os << str.c_str();
        return os;
    }
};

} // HE

namespace std
{
template<>
struct hash<HE::StaticString> final
{
    std::size_t operator() (const HE::StaticString& obj) const
    {
        return obj.GetID().value;
    }
};
} // std


#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace HE
{
    class StaticStringTest : public TestCollection
    {
    public:
        StaticStringTest() : TestCollection("StaticStringTest") {}

    protected:
        virtual void Prepare() override;
    };
} // HE
#endif //__UNIT_TEST__
