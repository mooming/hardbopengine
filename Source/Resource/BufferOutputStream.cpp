// Created by mooming.go@gmail.com, 2022

#include "BufferOutputStream.h"

#include "String/StringUtil.h"

namespace HE
{

static_assert(!std::is_copy_constructible<BufferOutputStream>::value);
static_assert(!std::is_copy_assignable<BufferOutputStream>::value);
static_assert(!std::is_move_constructible<BufferOutputStream>::value);
static_assert(!std::is_move_assignable<BufferOutputStream>::value);

BufferOutputStream::BufferOutputStream(Buffer& buffer)
    : buffer(buffer),
      cursor(0),
      errorCount(0),
      threadID(std::this_thread::get_id())
{
}

BufferOutputStream& BufferOutputStream::operator<<(char value)
{
    Put<char>(value);
    return *this;
}

BufferOutputStream& BufferOutputStream::operator<<(int8_t value)
{
    Put<int8_t>(value);
    return *this;
}

BufferOutputStream& BufferOutputStream::operator<<(uint8_t value)
{
    Put<uint8_t>(value);
    return *this;
}

BufferOutputStream& BufferOutputStream::operator<<(int16_t value)
{
    Put<int16_t>(value);
    return *this;
}

BufferOutputStream& BufferOutputStream::operator<<(uint16_t value)
{
    Put<uint16_t>(value);
    return *this;
}

BufferOutputStream& BufferOutputStream::operator<<(int32_t value)
{
    Put<int32_t>(value);
    return *this;
}

BufferOutputStream& BufferOutputStream::operator<<(uint32_t value)
{
    Put<uint32_t>(value);
    return *this;
}

BufferOutputStream& BufferOutputStream::operator<<(int64_t value)
{
    Put<int64_t>(value);
    return *this;
}

BufferOutputStream& BufferOutputStream::operator<<(uint64_t value)
{
    Put<uint64_t>(value);
    return *this;
}

#ifndef PLATFORM_LINUX
BufferOutputStream& BufferOutputStream::operator<<(size_t value)
{
    Put<size_t>(value);
    return *this;
}
#endif // PLATFORM_LINUX

BufferOutputStream& BufferOutputStream::operator<<(float value)
{
    Put<float>(value);
    return *this;
}

BufferOutputStream& BufferOutputStream::operator<<(double value)
{
    Put<double>(value);
    return *this;
}

BufferOutputStream& BufferOutputStream::operator<<(long double value)
{
    Put<long double>(value);
    return *this;
}

BufferOutputStream& BufferOutputStream::operator<<(const char* str)
{
    if (unlikely(str == nullptr))
    {
        return *this;
    }

    auto length = StringUtil::StrLen(str);
    Put<char>(str, length);

    return *this;
}

} // namespace HE

#ifdef __UNIT_TEST__
#include "BufferInputStream.h"
#include "BufferUtil.h"

namespace HE
{

BufferOutputStreamTest::BufferOutputStreamTest()
    : TestCollection(StringUtil::ToCompactClassName(__PRETTY_FUNCTION__))
{
}

void BufferOutputStreamTest::Prepare()
{
    using namespace BufferUtil;

    AddTest("Empty Buffer", [this](auto& ls) {
        Buffer buffer;
        BufferOutputStream bos(buffer);

        bos << 0;

        if (!bos.HasError())
        {
            ls << "The error should be occured when trying to"
               << " put something into the empty buffer" << lferr;
        }
    });

    AddTest("Memory Buffer", [this](auto& ls) {
        constexpr size_t TestCount = 128;

        auto buffer = GetMemoryBuffer<int>(TestCount, -1);
        BufferOutputStream bos(buffer);

        for (size_t i = 0; i < TestCount; ++i)
        {
            bos << i;
        }

        if (bos.HasError())
        {
            ls << "Unexpected error occured! Error Count = "
               << bos.GetErrorCount() << lferr;
        }

        bos.ClearErrorCount();

        bos << 0;
        bos << 0;
        bos << 0;

        if (!bos.HasError())
        {
            ls << "An error is not occured when exceeding its limit." << lferr;
        }

        if (bos.GetErrorCount() != 3)
        {
            ls << "Invalid error count " << bos.GetErrorCount()
               << ", 3 is expected." << lferr;
        }

        bos.ClearErrorCount();

        if (bos.GetErrorCount() != 0)
        {
            ls << "Invalid error count " << bos.GetErrorCount()
               << ", 0 is expected." << lferr;
        }

        int* intArray = reinterpret_cast<int*>(buffer.GetData());
        for (size_t i = 0; i < TestCount; ++i)
        {
            ls << i << "th value = " << intArray[i] << lf;

            if (intArray[i] != static_cast<int>(i))
            {
                ls << "Invalid value " << intArray[i] << ", " << i
                   << " is expected." << lferr;
            }
        }
    });

    AddTest("Array", [this](auto& ls) {
        constexpr size_t TestCount = 20;

        auto buffer = GetMemoryBuffer<int>(TestCount, -1);
        BufferOutputStream bos(buffer);

        {
            int intArray[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
            bos << intArray;
        }

        if (bos.HasError())
        {
            ls << "Unexpected error occured! Error Count = "
               << bos.GetErrorCount() << lferr;
        }

        bos.ClearErrorCount();

        if (bos.GetErrorCount() != 0)
        {
            ls << "Invalid error count " << bos.GetErrorCount()
               << ", 0 is expected." << lferr;
        }

        bos.ClearErrorCount();

        int* intArray =
            reinterpret_cast<int*>(buffer.GetData() + sizeof(size_t));
        for (int i = 0; i < 10; ++i)
        {
            ls << i << "th value = " << intArray[i] << lf;

            if (intArray[i] != i)
            {
                ls << "Invalid value " << intArray[i] << ", " << i
                   << " is expected." << lferr;
            }
        }

        if (bos.GetErrorCount() != 0)
        {
            ls << "Invalid error count " << bos.GetErrorCount()
               << ", 0 is expected." << lferr;
        }

        bos.ClearErrorCount();
    });

    AddTest("BufferInputStream", [this](auto& ls) {
        constexpr size_t TestCount = 20;

        auto buffer = GetMemoryBuffer<int>(TestCount, -1);
        BufferOutputStream bos(buffer);

        {
            int intArray[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
            bos << intArray;
        }

        if (bos.HasError())
        {
            ls << "Unexpected error occured! Error Count = "
               << bos.GetErrorCount() << lferr;
        }

        bos.ClearErrorCount();

        BufferInputStream bis(buffer);

        int intArray[10];
        bis >> intArray;

        if (bos.GetErrorCount() != 0)
        {
            ls << "Invalid error count " << bos.GetErrorCount()
               << ", 0 is expected." << lferr;
        }

        bos.ClearErrorCount();

        for (int i = 0; i < 10; ++i)
        {
            ls << i << " = " << intArray[i] << lf;

            if (intArray[i] != i)
            {
                ls << "Invalid value " << intArray[i] << ", " << i
                   << " is expected." << lferr;
            }
        }
    });
}

} // namespace HE
#endif //__UNIT_TEST__
