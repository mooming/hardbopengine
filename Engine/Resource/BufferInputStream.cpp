// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "BufferInputStream.h"


namespace hbe
{

	using This = BufferInputStream;

	BufferInputStream::BufferInputStream(const Buffer& buffer) noexcept :
		buffer(buffer), cursor(0), errorCount(0)
	{}

	This& BufferInputStream::operator>>(char& value) noexcept
	{
		get<char>(value, '\0');

		return *this;
	}

	This& BufferInputStream::operator>>(int8_t& value) noexcept
	{
		get<int8_t>(value, 0);

		return *this;
	}

	This& BufferInputStream::operator>>(uint8_t& value) noexcept
	{
		get<uint8_t>(value, 0);

		return *this;
	}

	This& BufferInputStream::operator>>(int16_t& value) noexcept
	{
		get<int16_t>(value, 0);

		return *this;
	}

	This& BufferInputStream::operator>>(uint16_t& value) noexcept
	{
		get<uint16_t>(value, 0);

		return *this;
	}

	This& BufferInputStream::operator>>(int32_t& value) noexcept
	{
		get<int32_t>(value, 0);

		return *this;
	}

	This& BufferInputStream::operator>>(uint32_t& value) noexcept
	{
		get<uint32_t>(value, 0);

		return *this;
	}

	This& BufferInputStream::operator>>(int64_t& value) noexcept
	{
		get<int64_t>(value, 0);

		return *this;
	}

	This& BufferInputStream::operator>>(uint64_t& value) noexcept
	{
		get<uint64_t>(value, 0);

		return *this;
	}

#ifndef PLATFORM_LINUX
	This& BufferInputStream::operator>>(size_t& value) noexcept
	{
		get<size_t>(value, 0);

		return *this;
	}
#endif // PLATFORM_LINUX

	This& BufferInputStream::operator>>(float& value) noexcept
	{
		get<float>(value, 0.0f);

		return *this;
	}

	This& BufferInputStream::operator>>(double& value) noexcept
	{
		get<double>(value, 0.0);

		return *this;
	}

	This& BufferInputStream::operator>>(long double& value) noexcept
	{
		get<long double>(value, 0);

		return *this;
	}

	This& BufferInputStream::operator>>(StaticString& str) noexcept
	{
		size_t length = 0;
		get<size_t>(length, 0);

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
			get<char>(ch, '\0');
			tmpStr.push_back(ch);
		}

		Assert(tmpStr.size() == length);

		str = StaticString(tmpStr.c_str());

		return *this;
	}

	This& BufferInputStream::operator>>(const hbe::HString& str) noexcept { return *this; }

} // namespace hbe

#ifdef __UNIT_TEST__
#include "Memory/MemoryManager.h"
#include "String/StringUtil.h"

namespace hbe
{

	BufferInputStreamTest::BufferInputStreamTest() : TestCollection(StringUtil::toCompactClassName(__PRETTY_FUNCTION__))
	{}

	void BufferInputStreamTest::prepare()
	{
		addTest("Empty Buffer", [this](auto& ls)
		{
			Buffer buffer;
			BufferInputStream bis(buffer);

			int value = 0;
			bis >> value;

			if (!bis.hasError())
			{
				ls << "The error should be occured when trying to"
				   << " get something from the empty buffer" << lferr;
			}
		});

		addTest("Memory Buffer", [this](auto& ls)
		{
			constexpr size_t TestCount = 128;
			constexpr size_t BufferSize = TestCount * sizeof(int);

			auto& mmgr = MemoryManager::getInstance();

			auto genFunc = [&](auto& size, auto& data)
			{
				size = BufferSize;
				auto intBuffer = mmgr.newArray<int>(TestCount);
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

				mmgr.deleteArray<int>((int*) data, TestCount);
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

			if (bis.hasError())
			{
				ls << "Unexpected error occured! Error Count = " << bis.getErrorCount() << lferr;
			}

			bis.clearErrorCount();

			{
				int value = 0;
				bis >> value;
				bis >> value;
				bis >> value;
			}

			if (!bis.hasError())
			{
				ls << "An error is not occured when exceeding its limit." << lferr;
			}

			if (bis.getErrorCount() != 3)
			{
				ls << "Invalid error count " << bis.getErrorCount() << ", 3 is expected." << lferr;
			}

			bis.clearErrorCount();

			if (bis.getErrorCount() != 0)
			{
				ls << "Invalid error count " << bis.getErrorCount() << ", 0 is expected." << lferr;
			}
		});
	}

} // namespace hbe
#endif //__UNIT_TEST__
