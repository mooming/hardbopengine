// Created by mooming.go@gmail.com, 2017

#pragma once

#include <cstdint>
#include <ostream>


namespace HE
{
    class String;

    class StaticString final
    {
        using TID = int32_t;

    private:
        TID id;

    public:
        StaticString();
        StaticString(const char* string);
        StaticString(const String& string);
        ~StaticString() = default;

        operator const char* () const;
        operator const String& () const;

        inline bool operator < (const StaticString& rhs) const { return id < rhs.id; }
        inline bool operator == (const StaticString& rhs) const { return id == rhs.id; }
        inline bool operator != (const StaticString& rhs) const { return id != rhs.id; }

        const char* c_str() const;
        const String& ToString() const;

        inline friend std::ostream& operator <<(std::ostream& os, const StaticString& str)
        {
            os << str.c_str();
            return os;
        }
    };
} // HE
