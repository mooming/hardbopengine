// Created by mooming.go@gmail.com, 2017

#include "String.h"

#include "Memory/MemoryManager.h"
#include "StringUtil.h"
#include "System/CommonUtil.h"
#include "System/Debug.h"
#include <cstdio>
#include <cstring>

namespace HE
{

String::String(const bool value)
    : hashCode(0)
{
    if (value)
    {
        buffer->resize(5);

        auto& text = *buffer;
        text[0] = 't';
        text[1] = 'r';
        text[2] = 'u';
        text[3] = 'e';
        text[4] = '\0';
    }
    else
    {
        buffer->resize(6);

        auto& text = *buffer;
        text[0] = 'f';
        text[1] = 'a';
        text[2] = 'l';
        text[3] = 's';
        text[4] = 'e';
        text[5] = '\0';
    }

    CalculateHashCode();
}

String::String(const Pointer ptr)
    : hashCode(0)
{
    buffer->resize(32);
    auto& text = *buffer;
    snprintf(text.data(), text.size(), "%p", ptr);
    CalculateHashCode();
}

String::String(const char letter)
    : hashCode(0)
{
    buffer->resize(2);
    auto& text = *buffer;
    text[0] = letter;
    text[1] = '\0';

    CalculateHashCode();
}

String::String(const unsigned char value)
    : hashCode(0)
{
    buffer->resize(16);
    auto& text = *buffer;
    snprintf(text.data(), text.size(), "0x%02X", value);
    buffer->resize(strlen(text.data()) + 1);

    CalculateHashCode();
}

String::String(const short value)
    : hashCode(0)
{
    buffer->resize(16);
    auto& text = *buffer;
    snprintf(text.data(), text.size(), "%d", value);
    buffer->resize(StringUtil::StrLen(text.data()) + 1);

    CalculateHashCode();
}

String::String(const unsigned short value)
    : hashCode(0)
{
    buffer->resize(16);
    auto& text = *buffer;
    snprintf(text.data(), text.size(), "%ud", value);
    buffer->resize(StringUtil::StrLen(text.data()) + 1);

    CalculateHashCode();
}

String::String(const int value)
    : hashCode(0)
{
    buffer->resize(16);
    auto& text = *buffer;
    snprintf(text.data(), text.capacity(), "%d", value);
    buffer->resize(StringUtil::StrLen(text.data()) + 1);

    CalculateHashCode();
}

String::String(const unsigned int value)
    : hashCode(0)
{
    buffer->resize(16);
    auto& text = *buffer;
    snprintf(text.data(), text.size(), "%u", value);
    buffer->resize(StringUtil::StrLen(text.data()) + 1);

    CalculateHashCode();
}

String::String(const long value)
    : hashCode(0)
{
    buffer->resize(16);
    auto& text = *buffer;
    snprintf(text.data(), text.size(), "%ld", value);
    buffer->resize(StringUtil::StrLen(text.data()) + 1);

    CalculateHashCode();
}

String::String(const unsigned long value)
    : hashCode(0)
{
    buffer->resize(16);
    auto& text = *buffer;
    snprintf(text.data(), text.size(), "%lu", value);
    buffer->resize(StringUtil::StrLen(text.data()) + 1);

    CalculateHashCode();
}

String::String(const long long value)
    : hashCode(0)
{
    buffer->resize(16);
    auto& text = *buffer;
    snprintf(text.data(), text.size(), "%lld", value);
    buffer->resize(StringUtil::StrLen(text.data()) + 1);

    CalculateHashCode();
}

String::String(const unsigned long long value)
    : hashCode(0)
{
    buffer->resize(16);
    auto& text = *buffer;
    snprintf(text.data(), text.size(), "%llu", value);
    buffer->resize(StringUtil::StrLen(text.data()) + 1);

    CalculateHashCode();
}

String::String(const float value)
    : hashCode(0)
{
    buffer->resize(16);
    auto& text = *buffer;
    snprintf(text.data(), text.size(), "%f", value);
    buffer->resize(StringUtil::StrLen(text.data()) + 1);

    CalculateHashCode();
}

String::String(const double value)
    : hashCode(0)
{
    buffer->resize(16);
    auto& text = *buffer;
    snprintf(text.data(), text.size(), "%f", value);
    buffer->resize(StringUtil::StrLen(text.data()) + 1);

    CalculateHashCode();
}

String::String(const long double value)
    : hashCode(0)
{
    buffer->resize(16);
    auto& text = *buffer;
    snprintf(text.data(), text.size(), "%Lf", value);
    buffer->resize(StringUtil::StrLen(text.data()) + 1);

    CalculateHashCode();
}

String::String(const char* text)
    : hashCode(0)
{
    const auto totalLength = strlen(text) + 1;
    buffer->resize(totalLength);
    Vector<Char>& textVec = *buffer;
    memcpy(textVec.data(), text, totalLength);

    CalculateHashCode();
}

String::String(const String& string, Index startIndex, Index endIndex)
    : buffer()
{
    if (startIndex >= string.Length())
    {
        startIndex = string.Length();
    }

    if (endIndex > string.Length())
    {
        endIndex = string.Length();
    }

    if (startIndex > endIndex)
    {
        startIndex = endIndex;
    }

    auto length = endIndex - startIndex;

    if (length > 0)
    {
        buffer->resize(length + 1);

        auto ptr = buffer->data();
        memcpy(ptr, string.buffer->data() + startIndex, length);
        ptr[length] = '\0';

        CalculateHashCode();
    }
    else
    {
        hashCode = 0;
    }
}

String& String::operator=(const char* text)
{
    if (buffer.GetReferenceCount() > 1)
    {
        Swap(String(text));
    }
    else
    {
        const auto textLength = strlen(text) + 1;
        buffer->resize(textLength);
        memcpy(buffer->data(), text, textLength);
    }

    return *this;
}

String& String::operator=(const String& rhs)
{
    if (buffer.GetReferenceCount() > 1)
    {
        Swap(String(rhs.ToCharArray()));
    }
    else
    {
        const auto length = rhs.buffer->size();
        buffer->resize(length);
        memcpy(buffer->data(), rhs.buffer->data(), length);
    }

    return *this;
}

bool String::operator<(const String& rhs) const
{
    const Index shorterLen = std::min(Length(), rhs.Length());
    Index matchCount = 0;

    for (Index i = 0; i < shorterLen; ++i)
    {
        if ((*buffer)[i] == (*rhs.buffer)[i])
        {
            ++matchCount;
            continue;
        }

        if ((*buffer)[i] > (*rhs.buffer)[i])
        {
            return false;
        }

        return true;
    }

    if (matchCount == shorterLen)
    {
        return Length() < rhs.Length();
    }

    return true;
}

bool String::operator==(const String& string) const
{
    if (hashCode != string.hashCode)
    {
        return false;
    }

    const auto length = Length();
    if (length != string.Length())
    {
        return false;
    }

    for (Index i = 0; i < length; ++i)
    {
        if ((*buffer)[i] != (*string.buffer)[i])
        {
            return false;
        }
    }

    return true;
}

const char* String::ToCharArray() const
{
    return buffer ? buffer.Get().data() : "";
}

String String::Clone() const
{
    String str;
    Assert(str.buffer);

    *(str.buffer) = *buffer;

    return str;
}

bool String::ContainsAt(const String& keyword, Index startIndex) const
{
    const Index endIndex = startIndex + keyword.Length();

    if (endIndex > Length())
    {
        return false;
    }

    Index index = 0;

    for (Index i = startIndex; i < endIndex; ++i, ++index)
    {
        if ((*buffer)[i] != (*keyword.buffer)[index])
        {
            return false;
        }
    }

    return true;
}

Index String::Find(const Char ch) const
{
    const auto length = Length();

    for (Index i = 0; i < length; ++i)
    {
        if ((*buffer)[i] == ch)
        {
            return i;
        }
    }

    return length;
}

Index String::Find(const Array<Char>& chs) const
{
    const auto length = Length();
    auto chsLen = chs.Size();

    for (Index i = 0; i < length; ++i)
    {
        for (decltype(chsLen) j = 0; j < chsLen; ++j)
        {
            if ((*buffer)[i] == chs[j])
            {
                return i;
            }
        }
    }

    return length;
}

Index String::Find(const String& keyword) const
{
    const auto length = Length();
    const auto keywordLength = Length();

    if (keywordLength > length)
    {
        return length;
    }

    const Index lastIndex = length - keywordLength + 1;
    for (Index i = 0; i < lastIndex; ++i)
    {
        if (ContainsAt(keyword, i))
        {
            return i;
        }
    }
    return length;
}

Index String::Find(
    const String& keyword, Index startIndex, Index endIndex) const
{
    const auto length = Length();
    const auto keywordLength = Length();

    Assert(startIndex < length);
    if (startIndex >= length)
    {
        startIndex = length - 1;
    }

    Assert(endIndex >= startIndex);
    if (endIndex < startIndex)
    {
        endIndex = startIndex;
    }

    Assert(endIndex <= length);
    if (endIndex > length)
    {
        endIndex = length;
    }

    if ((startIndex + keywordLength) > endIndex)
    {
        return length;
    }

    const Index lastIndex = endIndex - keywordLength + 1;
    for (Index i = startIndex; i < lastIndex; ++i)
    {
        if (ContainsAt(keyword, i))
        {
            return i;
        }
    }

    return length;
}

Index String::FindLast(const Char ch) const
{
    const auto length = Length();
    for (Index i = length; i > 0;)
    {
        if ((*buffer)[--i] == ch)
        {
            return i;
        }
    }

    return length;
}

String String::Append(const Char letter) const
{
    String str;
    const auto length = Length();

    str.buffer->resize(length + sizeof(Char) + 1);
    memcpy(str.buffer->data(), buffer->data(), length);

    (*str.buffer)[length] = letter;
    (*str.buffer)[length + 1] = '\0';

    return str;
}

String String::Append(const int value) const
{
    char tmp[16];
    snprintf(tmp, sizeof(tmp), "%d", value);

    const auto length = Length();
    const Index tmpLength = static_cast<Index>(strlen(tmp));

    String str;
    str.buffer->resize(length + tmpLength + 1);

    memcpy(str.buffer->data(), buffer->data(), length);
    memcpy(str.buffer->data() + length, tmp, tmpLength + 1);

    return str;
}

String String::Append(const float value) const
{
    char tmp[16];
    snprintf(tmp, sizeof(tmp), "%f", value);

    const auto length = Length();
    const Index tmpLength = static_cast<Index>(strlen(tmp));

    String str;
    str.buffer->resize(length + tmpLength + 1);

    memcpy(str.buffer->data(), buffer->data(), length);
    memcpy(str.buffer->data() + length, tmp, tmpLength + 1);

    return str;
}

String String::Append(const Char* text) const
{
    const auto length = Length();
    const Index textLength = static_cast<Index>(strlen(text));

    String str;
    str.buffer->resize(length + textLength + 1);

    memcpy(str.buffer->data(), buffer->data(), length);
    memcpy(str.buffer->data() + length, text, textLength + 1);

    return str;
}

String String::Append(const String& string) const
{
    if (string.IsEmpty())
    {
        return Clone();
    }

    const auto length = Length();
    const auto strLength = string.Length();

    String str;
    str.buffer->resize(length + strLength + 1);

    memcpy(str.buffer->data(), buffer->data(), length);
    memcpy(str.buffer->data() + length, string.buffer->data(), strLength + 1);

    return str;
}

void String::AppendSelf(const Char letter)
{
    const auto index = Length();
    buffer->push_back('\0');
    buffer.Get()[index] = letter;
}

void String::AppendSelf(const int value)
{
    char tmp[16];
    snprintf(tmp, sizeof(tmp), "%d", value);

    const auto length = Length();
    const Index tmpLength = static_cast<Index>(strlen(tmp));
    const auto newLength = length + tmpLength + 1;

    if (newLength > buffer->capacity())
    {
        buffer->reserve(newLength * 3 / 2);
    }

    buffer->resize(newLength);
    memcpy(buffer->data() + length, tmp, tmpLength + 1);
}

void String::AppendSelf(const float value)
{
    char tmp[16];
    snprintf(tmp, sizeof(tmp), "%f", value);

    const auto length = Length();
    const Index tmpLength = static_cast<Index>(strlen(tmp));
    const auto newLength = length + tmpLength + 1;

    if (newLength > buffer->capacity())
    {
        buffer->reserve(newLength * 3 / 2);
    }

    buffer->resize(newLength);
    memcpy(buffer->data() + length, tmp, tmpLength + 1);
}

void String::AppendSelf(const Char* text)
{
    const auto length = Length();
    const Index textLength = static_cast<Index>(strlen(text));
    ;
    const auto newLength = length + textLength + 1;

    if (newLength > buffer->capacity())
    {
        buffer->reserve(newLength * 3 / 2);
    }

    buffer->resize(newLength);
    memcpy(buffer->data() + length, text, textLength + 1);
}

void String::AppendSelf(const String& string)
{
    if (string.IsEmpty())
    {
        return;
    }

    const auto length = Length();
    const Index textLength = string.Length();
    const auto newLength = length + textLength + 1;

    if (newLength > buffer->capacity())
    {
        buffer->reserve(newLength * 3 / 2);
    }

    buffer->resize(newLength);
    memcpy(buffer->data() + length, string.buffer->data(), textLength + 1);
}

String String::Replace(
    const String& from, const String& to, Index offset, Index endIndex) const
{
    // TODO - NOT IMPLEMENTED YET

    Assert(false);
    return String();
}

String String::ReplaceAll(char from, char to) const
{
    if (!buffer)
    {
        return String();
    }

    String str = Clone();
    Char* data = str.buffer->data();
    Assert(data != nullptr);

    Index length = str.Length();
    for (Index i = 0; i < length; ++i)
    {
        if (data[i] == from)
        {
            data[i] = to;
        }
    }

    str.CalculateHashCode();

    return str;
}

String String::ReplaceAll(String from, String to) const
{
    // TODO - NOT IMPLEMENTED YET

    Assert(false);
    return String();
}

void String::ParseKeyValue(String& key, String& value)
{
    auto index = Find("=");
    key = SubString(0, index).Trim();
    value = SubString(index + 1).Trim();
}

void String::CalculateHashCode()
{
    hashCode = 5381;

    const auto length = Length();
    auto text = buffer->data();

    Assert(length != 0 || text[0] == '\0');

    for (Index i = 0; i < length; ++i)
    {
        Index ch = text[i];
        hashCode = ((hashCode << 5) + hashCode) + ch; /* hash * 33 + c */
    }
}

void String::ResetBuffer(size_t size)
{
    buffer->reserve(static_cast<Index>(size + 1));
    buffer->clear();
    buffer->push_back('\0');
}

} // namespace HE

#ifdef __UNIT_TEST__
#include "System/ScopedTime.h"

namespace HE
{

void StringTest::Prepare()
{
    AddTest("Comparison with Zero-Terminated String", [this](auto& ls) {
        String str = "Hello? World!";
        ls << str.c_str() << lf;

        if (str != "Hello? World!")
        {
            ls << "String Compare Failure. " << str << lferr;
        }
    });

    AddTest("To Lower Case", [this](auto& ls) {
        String str("Hello? World!");

        auto lower = str.GetLowerCase();
        ls << lower.c_str() << lf;

        if (lower != "hello? world!")
        {
            ls << "To lowercase failed. " << lower << lferr;
        }
    });

    AddTest("To Upper Case", [this](auto& ls) {
        String str("Hello? World!");

        auto upper = str.GetUpperCase();
        ls << upper.c_str() << lf;

        if (upper != "HELLO? WORLD!")
        {
            ls << "To uppercase failed. " << upper << lferr;
        }
    });

    AddTest("Move Semantics", [this](auto& ls) {
        String str("Hello? World!");

        auto upper = str.GetUpperCase();
        auto tmpString = std::move(upper);
        ls << tmpString.c_str() << lf;

        if (tmpString != "HELLO? WORLD!")
        {
            ls << "String move failed." << lferr;
        }
    });

    AddTest("Find Last", [this](auto& ls) {
        String str("Hello? World!");
        auto lastL = str.FindLast('l');
        if (lastL != 10)
        {
            ls << "Failed to find the last 'l', index = " << lastL
               << ", but expected 10." << lferr;
        }
    });

    AddTest("SubString", [this](auto& ls) {
        String str("Hello? World!");
        auto lastL = str.FindLast('l');
        auto afterL = str.SubString(lastL);
        ls << afterL.c_str() << lf;

        if (afterL != "ld!")
        {
            ls << "Substring failed: " << afterL << lferr;
        }
    });

    AddTest("Performance", [this](auto& ls) {
        constexpr int COUNT = 100000;

        Time::TDuration heTime;

        {
            Time::ScopedTime measure(heTime);

            String str;
            for (int i = 0; i < COUNT; ++i)
            {
                str = "";
                for (char ch = 'a'; ch <= 'z'; ++ch)
                {
                    str += ch;
                }
            }
        }

        Time::TDuration stlTime;

        {
            Time::ScopedTime measure(stlTime);

            std::string str;
            for (int i = 0; i < COUNT; ++i)
            {
                str = "";
                for (char ch = 'a'; ch <= 'z'; ++ch)
                {
                    str += ch;
                }
            }
        }

        ls << "Time: he = " << Time::ToFloat(heTime)
           << ", stl = " << Time::ToFloat(stlTime) << lf;

        if (heTime > stlTime)
        {
            ls << "HE String is slower than STL string." << std::endl
               << "Time: he = " << Time::ToFloat(heTime)
               << ", stl = " << Time::ToFloat(stlTime) << lfwarn;
        }
    });
}

} // namespace HE
#endif //__UNIT_TEST__
