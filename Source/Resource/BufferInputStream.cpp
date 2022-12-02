// Created by mooming.go@gmail.com, 2022

#include "BufferInputStream.h"


namespace HE
{

using This = BufferInputStream;

BufferInputStream::BufferInputStream(const Buffer& buffer)
    : buffer(buffer)
    , cursor(0)
    , errorCount(0)
{
}

This& BufferInputStream::operator >> (char& value)
{
    Get<char>(value, '\0');
    return *this;
}

This& BufferInputStream::operator >> (int8_t& value)
{
    Get<int8_t>(value, 0);
    return *this;
}

This& BufferInputStream::operator >> (uint8_t& value)
{
    Get<uint8_t>(value, 0);
    return *this;
}

This& BufferInputStream::operator >> (int16_t& value)
{
    Get<int16_t>(value, 0);
    return *this;
}

This& BufferInputStream::operator >> (uint16_t& value)
{
    Get<uint16_t>(value, 0);
    return *this;
}

This& BufferInputStream::operator >> (int32_t& value)
{
    Get<int32_t>(value, 0);
    return *this;
}

This& BufferInputStream::operator >> (uint32_t& value)
{
    Get<uint32_t>(value, 0);
    return *this;
}

This& BufferInputStream::operator >> (int64_t& value)
{
    Get<int64_t>(value, 0);
    return *this;
}

This& BufferInputStream::operator >> (uint64_t& value)
{
    Get<uint64_t>(value, 0);
    return *this;
}

This& BufferInputStream::operator >> (size_t& value)
{
    Get<size_t>(value, 0);
    return *this;
}

This& BufferInputStream::operator >> (float& value)
{
    Get<float>(value, 0.0f);
    return *this;
}

This& BufferInputStream::operator >> (double& value)
{
    Get<double>(value, 0.0);
    return *this;
}

This& BufferInputStream::operator >> (long double& value)
{
    Get<long double>(value, 0);
    return *this;
}

This& BufferInputStream::operator >> (StaticString& str)
{
    size_t length = 0;
    Get<size_t>(length, 0);

    if (length <= 0)
    {
        const static StaticString zeroStr("");
        str = zeroStr;
        return *this;
    }

    using namespace HSTL;

    constexpr size_t InlineBufferSize = 256;
    HInlineString<InlineBufferSize> tmpStr;
    tmpStr.reserve(length);

    char ch = '\0';

    for (size_t i = 0; i < length; ++i)
    {
        Get<char>(ch, '\0');
        tmpStr.push_back(ch);
    }

    Assert(tmpStr.size() == length);
    
    str = StaticString(tmpStr.c_str());

    return *this;
}

This& BufferInputStream::operator >> (const HSTL::HString& str)
{
    return *this;
}

} // HE

#ifdef __UNIT_TEST__
#include "Memory/MemoryManager.h"
#include "String/StringUtil.h"


namespace HE
{

BufferInputStreamTest::BufferInputStreamTest()
    : TestCollection(StringUtil::ToCompactClassName(__PRETTY_FUNCTION__))
{
}

void BufferInputStreamTest::Prepare()
{
    AddTest("Empty Buffer", [this](auto& ls)
    {
        Buffer buffer;
        BufferInputStream bis(buffer);

        int value = 0;
        bis >> value;

        if (!bis.HasError())
        {
            ls << "The error should be occured when trying to"
                << " get something from the empty buffer" << lferr;
        }
    });

    AddTest("Memory Buffer", [this](auto& ls)
    {
        constexpr size_t TestCount = 128;
        constexpr size_t BufferSize = TestCount * sizeof(int);

        auto& mmgr = MemoryManager::GetInstance();

        auto genFunc = [&](auto& size, auto& data)
        {
            size = BufferSize;
            auto intBuffer = mmgr.NewArray<int>(TestCount);
            for (int i = 0; i < TestCount; ++i)
            {
                intBuffer[i] = i;
            }

            data = reinterpret_cast<Buffer::TBufferData>(intBuffer);
        };

        auto relFunc = [&](auto size, auto data)
        {
            if (size != BufferSize)
            {
                ls << "Invalid size " << size << ", " << TestCount
                    << " is expected." << lferr;
                return;
            }

            if (data == nullptr)
            {
                ls << "Invalid data " << (void*)data << lferr;
                return;
            }

            mmgr.DeleteArray<int>((int*)data, TestCount);
        };

        Buffer buffer(genFunc, relFunc);
        BufferInputStream bis(buffer);

        for (int i = 0; i < TestCount; ++i)
        {
            int value = 0;
            bis >> value;

            ls << i << "th value = " << value << lf;

            if (value != i)
            {
                ls << "Invalid value " << value
                    << ", but " << i << " is expected." << lferr;
            }
        }

        if (bis.HasError())
        {
            ls << "Unexpected error occured! Error Count = "
                << bis.GetErrorCount() << lferr;
        }

        bis.ClearErrorCount();

        {
            int value = 0;
            bis >> value;
            bis >> value;
            bis >> value;
        }

        if (!bis.HasError())
        {
            ls << "An error is not occured when exceeding its limit." << lferr;
        }

        if (bis.GetErrorCount() != 3)
        {
            ls << "Invalid error count " << bis.GetErrorCount()
                << ", 3 is expected." << lferr;
        }

        bis.ClearErrorCount();

        if (bis.GetErrorCount() != 0)
        {
            ls << "Invalid error count " << bis.GetErrorCount()
                << ", 0 is expected." << lferr;
        }
    });
}

} // HE
#endif //__UNIT_TEST__
