// Created by mooming.go@gmail.com, 2017

#pragma once

#include "Array.h"
#include "Letter.h"
#include "Shareable.h"
#include "Types.h"
#include "Vector.h"

#include <string>

namespace HE
{
    class String
    {
    public:
        using Char = char;
		//using Index = int;

    private:
        Shareable<Vector<Char>> buffer;
        Index hashCode;

    public:
        String() : buffer(), hashCode(0)
        {
            buffer->push_back('\0');
        }

        String(String& rhs) : buffer(rhs.buffer), hashCode(rhs.hashCode)
        {
        }

        String(String&& rhs) : buffer(std::move(rhs.buffer)), hashCode(rhs.hashCode)
        {
            rhs.hashCode = 0;
        }

        String(const bool value);
        String(const Pointer ptr);
        String(const char letter);
        String(const unsigned char value);
        String(const short value);
        String(const unsigned short value);
        String(const int value);
        String(const unsigned int value);
        String(const long value);
        String(const unsigned long value);
        String(const long long value);
        String(const unsigned long long value);
        String(const float value);
        String(const double value);
        String(const long double value);
        String(const char* text);

        String(const std::string str) : String(str.c_str()) {}
        String(const String& string, Index startIndex, Index endIndex = -1);

        inline String& operator= (String&& rhs)
        {
            Swap(std::move(rhs));
            return *this;
        }

        String& operator= (const char* text);
        String& operator= (const String& rhs);

        bool operator<(const String & rhs) const;
        inline bool operator> (const String& rhs) const { return rhs < *this; }
        inline bool operator<=(const String& rhs) const { return !(*this > rhs); }
        inline bool operator>=(const String& rhs) const { return !(*this < rhs); }

        bool operator== (const String& rhs) const;
        inline bool operator!= (const String& rhs) const { return !(*this == rhs); }

        inline bool operator== (const char* rhs) const { return *this == String(rhs); }
        inline bool operator!= (const char* rhs) const { return !(*this == rhs); }

        inline bool operator== (std::nullptr_t) const { return buffer->empty(); }
        inline bool operator!= (std::nullptr_t) const { return !buffer->empty(); }

        String operator+ (String str)
        {
            return Append(str);
        }

        template <typename U>
        void operator+= (U str)
        {
            AppendSelf(str);
        }

        inline operator const char* () const
        {
            return ToCharArray();
        }

        inline Index Length() const
        {
            return buffer->size() - 1;
        }

        inline bool IsEmpty() const
        {
            return buffer->empty();
        }

        inline Index HashCode() const
        {
            return hashCode;
        }

        String Clone() const;

        inline String SubString(Index startIndex, Index endIndex = -1) const
        {
            return String(*this, startIndex, endIndex);
        }

        bool ContainsAt(const String& keyword, Index startIndex) const;

        Index Find(const Char ch) const;
        Index Find(const Array<Char>& chs) const;
        Index Find(const String& keyword) const;
        Index Find(const String& keyword, Index startIndex, Index endIndex = -1) const;

        Index FindLast(const Char ch) const;

        bool StartsWith(const Char ch) const
        {
            if (IsEmpty())
                return false;
            return (*buffer)[0] == ch;
        }

        bool StartsWith(const String& header) const
        {
            return ContainsAt(header, 0);
        }

        bool EndsWith(const Char ch) const
        {
            if (IsEmpty())
                return false;
            return (*buffer)[Length() - 1] == ch;
        }

        bool EndsWith(const String& tail) const
        {
            if (Length() < tail.Length())
                return false;
            return ContainsAt(tail, Length() - tail.Length());
        }

        inline bool Contains(const String& keyword) const
        {
            return Find(keyword) >= Length();
        }

        String Append(const Char letter) const;
        String Append(const int value) const;
        String Append(const float value) const;
        String Append(const Char* text) const;
        String Append(const String& string) const;

        void AppendSelf(const Char letter);
        void AppendSelf(const int value);
        void AppendSelf(const float value);
        void AppendSelf(const Char* text);
        void AppendSelf(const String& string);

        String Replace(const String& from, const String& to, Index offset = 0, Index endIndex = -1)  const;
        String ReplaceAll(char from, char to) const;
        String ReplaceAll(String from, String to) const;

        inline String Trim() const
        {
            Index startIndex = -1;
            Index endIndex = -1;

            const auto length = Length();
            for (Index i = 0; i < length; ++i)
            {
                if (Letter::IsGenuineLetter((*buffer)[i]))
                {
                    if (startIndex >= length)
                    {
                        startIndex = i;
                    }
                    endIndex = i;
                }
            }

            if (startIndex >= length)
                return String();

            return SubString(startIndex, endIndex + 1);
        }

        String Head() const
        {
            Index index = Find(Array<Char>({ ' ', '\t', '\n', '\r' }));
            if (index < Length())
            {
                return SubString(0, index);
            }

            return *this;
        }

        String ExceptHead() const
        {
            Index index = Find(Array<Char>({ ' ', '\t', '\n', '\r' }));
            if (index < Length())
            {
                return SubString(index);
            }
            return String();
        }

        inline Char* GetBuffer() { return buffer->data(); }
        inline const Char* GetBuffer() const { return buffer->data(); }
        void ResetBuffer(size_t size);

        inline void Swap(String&& target)
        {
            Index tmpHashCode = hashCode;
            hashCode = target.hashCode;
            target.hashCode = tmpHashCode;

            buffer.Swap(target.buffer);
        }

        inline void ToLowerCase()
        {
            constexpr Char diff = 'a' - 'A';

            auto tmp = GetBuffer();
            const auto length = Length();
            for (Index i = 0; i < length; ++i)
            {
                if (Letter::IsUpperCase(tmp[i]))
                    tmp[i] += diff;
            }

            CalculateHashCode();
        }

        inline String GetLowerCase() const
        {
            String str = Clone();
            str.ToLowerCase();
            return str;
        }

        inline void ToUpperCase()
        {
            constexpr Char diff = 'A' - 'a';

            auto tmp = GetBuffer();
            const auto length = Length();
            for (Index i = 0; i < length; ++i)
            {
                if (Letter::IsLowerCase(tmp[i]))
                    tmp[i] += diff;
            }

            CalculateHashCode();
        }

        inline String GetUpperCase() const
        {
            String str = Clone();
            str.ToUpperCase();
            return str;
        }

        const char* ToCharArray() const;

        inline char ToChar() const
        {
            return buffer ? (*buffer)[0] : '\0';
        }

        inline char ToUnsignedChar() const
        {
            return buffer ? static_cast<unsigned char>((*buffer)[0]) : 0;
        }

        inline int ToInt() const
        {
            return buffer ? std::stoi(buffer->data()) : 0;
        }

        inline unsigned int ToUnsignedInt() const
        {
            return buffer ? static_cast<unsigned int>(std::stoul(buffer->data())) : 0;
        }

        inline long ToLong() const
        {
            return buffer ? std::stol(buffer->data()) : 0;
        }

        inline unsigned long ToUnsignedLong() const
        {
            return buffer ? std::stoul(buffer->data()) : 0;
        }

        inline long long ToLongLong() const
        {
            return buffer ? std::stoll(buffer->data()) : 0;
        }

        inline unsigned long long ToUnsignedLongLong() const
        {
            return buffer ? std::stoull(buffer->data()) : 0;
        }

        inline float ToFloat() const
        {
            return buffer ? std::stof(buffer->data()) : 0.0f;
        }

        inline double ToDouble() const
        {
            return buffer ? std::stod(buffer->data()) : 0.0;
        }

        inline long double ToLongDouble() const
        {
            return buffer ? std::stold(buffer->data()) : 0.0;
        }

        inline void* ToPointer() const
        {
            if (buffer)
            {
                unsigned long long address = std::stoull(buffer->data(), 0, 16);
                return reinterpret_cast<void*>(address);
            }
            return nullptr;
        }

        void ParseKeyValue(String& key, String& value);

    private:
        void CalculateHashCode();
    };

}


#ifdef __UNIT_TEST__
#include "TestCase.h"
namespace HE
{
    class StringTest : public TestCase
    {
    public:
        StringTest() : TestCase("StringTest") {}

    protected:
        virtual bool DoTest() override;
    };
}
#endif //__UNIT_TEST__
