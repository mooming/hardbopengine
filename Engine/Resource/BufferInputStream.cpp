// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "BufferInputStream.h"


namespace hbe
{
using This = BufferInputStream;

BufferInputStream::BufferInputStream(const Buffer& buffer) noexcept
	: buffer(buffer)
	, cursor(0)
	, errorCount(0)
{
}

This& BufferInputStream::operator>>(char& value) noexcept
{
	Get<char>(value, '\0');

	return *this;
}

This& BufferInputStream::operator>>(int8_t& value) noexcept
{
	Get<int8_t>(value, 0);

	return *this;
}

This& BufferInputStream::operator>>(uint8_t& value) noexcept
{
	Get<uint8_t>(value, 0);

	return *this;
}

This& BufferInputStream::operator>>(int16_t& value) noexcept
{
	Get<int16_t>(value, 0);

	return *this;
}

This& BufferInputStream::operator>>(uint16_t& value) noexcept
{
	Get<uint16_t>(value, 0);

	return *this;
}

This& BufferInputStream::operator>>(int32_t& value) noexcept
{
	Get<int32_t>(value, 0);

	return *this;
}

This& BufferInputStream::operator>>(uint32_t& value) noexcept
{
	Get<uint32_t>(value, 0);

	return *this;
}

This& BufferInputStream::operator>>(int64_t& value) noexcept
{
	Get<int64_t>(value, 0);

	return *this;
}

This& BufferInputStream::operator>>(uint64_t& value) noexcept
{
	Get<uint64_t>(value, 0);

	return *this;
}

#ifndef PLATFORM_LINUX
This& BufferInputStream::operator>>(size_t& value) noexcept
{
	Get<size_t>(value, 0);

	return *this;
}
#endif // PLATFORM_LINUX

This& BufferInputStream::operator>>(float& value) noexcept
{
	Get<float>(value, 0.0f);

	return *this;
}

This& BufferInputStream::operator>>(double& value) noexcept
{
	Get<double>(value, 0.0);

	return *this;
}

This& BufferInputStream::operator>>(long double& value) noexcept
{
	Get<long double>(value, 0);

	return *this;
}

This& BufferInputStream::operator>>(StaticString& str) noexcept
{
	size_t length = 0;
	Get<size_t>(length, 0);

	if (length <= 0)
	{
		const static StaticString zeroStr("");
		str = zeroStr;

		return *this;
	}

	using namespace hbe;

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

This& BufferInputStream::operator>>(const hbe::HString& str) noexcept
{
	return *this;
}
} // namespace hbe

#ifdef TEST_ENABLED
#include "BufferOutputStream.h"
#include "BufferUtil.h"
#include "Memory/MemoryManager.h"
#include "String/StringUtil.h"

namespace hbe
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
			for (size_t i = 0; i < TestCount; ++i)
			{
				intBuffer[i] = i;
			}

			data = reinterpret_cast<Buffer::TBufferData>(intBuffer);
		};

		auto relFunc = [&](auto size, auto data)
		{
			if (size != BufferSize)
			{
				ls << "Invalid size " << size << ", " << TestCount << " is expected." << lferr;

				return;
			}

			if (data == nullptr)
			{
				ls << "Invalid data " << (void*) data << lferr;

				return;
			}

			mmgr.DeleteArray<int>((int*) data, TestCount);
		};

		Buffer buffer(genFunc, relFunc);
		BufferInputStream bis(buffer);

		for (size_t i = 0; i < TestCount; ++i)
		{
			int value = 0;
			bis >> value;

			ls << i << "th value = " << value << lf;

			if (value != static_cast<int>(i))
			{
				ls << "Invalid value " << value << ", but " << i << " is expected." << lferr;
			}
		}

		if (bis.HasError())
		{
			ls << "Unexpected error occured! Error Count = " << bis.GetErrorCount() << lferr;
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
			ls << "Invalid error count " << bis.GetErrorCount() << ", 3 is expected." << lferr;
		}

		bis.ClearErrorCount();

		if (bis.GetErrorCount() != 0)
		{
			ls << "Invalid error count " << bis.GetErrorCount() << ", 0 is expected." << lferr;
		}
	});

	AddTest("Tail Shorter Than One Element", [this](auto& ls)
	{
		Buffer buffer = BufferUtil::GetMemoryBuffer<uint8_t>(6, 0);
		BufferInputStream bis(buffer);

		int first = 0;
		bis >> first;

		if (bis.HasError())
		{
			ls << "A 4 byte read at offset 0 of a 6 byte buffer must be honoured." << lferr;
		}

		int second = 0;
		bis >> second;

		if (!bis.HasError())
		{
			ls << "A read ending at byte 8 of a 6 byte buffer must be refused." << lferr;
		}

		if (bis.GetErrorCount() != 1)
		{
			ls << "Invalid error count " << bis.GetErrorCount() << ", 1 is expected." << lferr;
		}
	});

	AddTest("Bulk Read Past The End", [this](auto& ls)
	{
		constexpr size_t BufferSize = 64;
		constexpr size_t ElementCount = 20;

		Buffer buffer = BufferUtil::GetMemoryBuffer<uint8_t>(BufferSize, 0);
		*reinterpret_cast<size_t*>(buffer.GetData()) = ElementCount;

		int values[ElementCount];
		for (size_t i = 0; i < ElementCount; ++i)
		{
			values[i] = -1;
		}

		BufferInputStream bis(buffer);
		bis >> values;

		if (!bis.HasError())
		{
			ls << ElementCount << " ints from offset 8 cannot fit " << BufferSize << " bytes." << lferr;
		}

		for (size_t i = 0; i < ElementCount; ++i)
		{
			if (values[i] != -1)
			{
				ls << "A refused read must not write the destination, element " << i << " holds " << values[i] << lferr;
				break;
			}
		}
	});

	AddTest("Refused Read Keeps The Container", [this](auto& ls)
	{
		constexpr size_t BufferSize = 64;

		HVector<int> values;
		values.push_back(111);
		values.push_back(222);
		values.push_back(333);
		{
			Buffer buffer = BufferUtil::GetMemoryBuffer<uint8_t>(4, 0);
			BufferInputStream bis(buffer);

			bis.operator>> <int>(values);

			if (!bis.HasError())
			{
				ls << "A size_t prefix cannot be read out of a 4 byte buffer." << lferr;
			}
		}
		{
			Buffer buffer = BufferUtil::GetMemoryBuffer<uint8_t>(BufferSize, 0);
			BufferInputStream bis(buffer);

			bis.operator>> <int>(values);

			if (bis.HasError())
			{
				ls << "A zero length prefix inside the buffer is an honoured empty array, not a refusal." << lferr;
			}
		}

		if (!values.empty())
		{
			ls << "An empty array in the image must clear the container, it still holds " << values.size()
			   << " element(s)." << lferr;
		}
		{
			Buffer buffer = BufferUtil::GetMemoryBuffer<uint8_t>(BufferSize, 0);
			BufferInputStream bis(buffer);

			int payload[3] = {7, 8, 9};
			BufferOutputStream bos(buffer);
			bos << payload;

			bis.operator>> <int>(values);

			if (bis.HasError())
			{
				ls << "A 3 element array written by the output stream must read back." << lferr;
			}

			if (values.size() != 3 || values[0] != 7 || values[2] != 9)
			{
				ls << "An accepted read must replace the container contents, it holds " << values.size()
				   << " element(s)." << lferr;
			}
		}
	});
}
} // namespace hbe
#endif //TEST_ENABLED
