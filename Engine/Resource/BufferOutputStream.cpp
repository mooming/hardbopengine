// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "BufferOutputStream.h"

#include "String/StringUtil.h"


namespace hbe
{

	static_assert(!std::is_copy_constructible<BufferOutputStream>::value);
	static_assert(!std::is_copy_assignable<BufferOutputStream>::value);
	static_assert(!std::is_move_constructible<BufferOutputStream>::value);
	static_assert(!std::is_move_assignable<BufferOutputStream>::value);

	BufferOutputStream::BufferOutputStream(Buffer& buffer) noexcept :
		buffer(buffer), cursor(0), errorCount(0), threadID(std::this_thread::get_id())
	{}

	BufferOutputStream& BufferOutputStream::operator<<(char value) noexcept
	{
		put<char>(value);

		return *this;
	}

	BufferOutputStream& BufferOutputStream::operator<<(int8_t value) noexcept
	{
		put<int8_t>(value);

		return *this;
	}

	BufferOutputStream& BufferOutputStream::operator<<(uint8_t value) noexcept
	{
		put<uint8_t>(value);

		return *this;
	}

	BufferOutputStream& BufferOutputStream::operator<<(int16_t value) noexcept
	{
		put<int16_t>(value);

		return *this;
	}

	BufferOutputStream& BufferOutputStream::operator<<(uint16_t value) noexcept
	{
		put<uint16_t>(value);

		return *this;
	}

	BufferOutputStream& BufferOutputStream::operator<<(int32_t value) noexcept
	{
		put<int32_t>(value);

		return *this;
	}

	BufferOutputStream& BufferOutputStream::operator<<(uint32_t value) noexcept
	{
		put<uint32_t>(value);

		return *this;
	}

	BufferOutputStream& BufferOutputStream::operator<<(int64_t value) noexcept
	{
		put<int64_t>(value);

		return *this;
	}

	BufferOutputStream& BufferOutputStream::operator<<(uint64_t value) noexcept
	{
		put<uint64_t>(value);

		return *this;
	}

#ifndef PLATFORM_LINUX
	BufferOutputStream& BufferOutputStream::operator<<(size_t value) noexcept
	{
		put<size_t>(value);

		return *this;
	}
#endif // PLATFORM_LINUX

	BufferOutputStream& BufferOutputStream::operator<<(float value) noexcept
	{
		put<float>(value);

		return *this;
	}

	BufferOutputStream& BufferOutputStream::operator<<(double value) noexcept
	{
		put<double>(value);

		return *this;
	}

	BufferOutputStream& BufferOutputStream::operator<<(long double value) noexcept
	{
		put<long double>(value);

		return *this;
	}

	BufferOutputStream& BufferOutputStream::operator<<(const char* str) noexcept
	{
		if (unlikely(str == nullptr)) return *this;

		auto length = StringUtil::strLen(str);
		put<char>(str, length);

		return *this;
	}

} // namespace hbe

#ifdef __UNIT_TEST__
#include "BufferInputStream.h"
#include "BufferUtil.h"

namespace hbe
{

	BufferOutputStreamTest::BufferOutputStreamTest() :
		TestCollection(StringUtil::toCompactClassName(__PRETTY_FUNCTION__))
	{}

	void BufferOutputStreamTest::prepare()
	{
		using namespace BufferUtil;

		addTest("Empty Buffer", [this](auto& ls)
		{
			Buffer buffer;
			BufferOutputStream bos(buffer);

			bos << 0;

			if (!bos.hasError())
			{
				ls << "The error should be occurred when trying to"
				   << " put something into the empty buffer" << lferr;
			}
		});

		addTest("Memory Buffer", [this](auto& ls)
		{
			constexpr size_t TestCount = 128;

			auto buffer = getMemoryBuffer<int>(TestCount, -1);
			BufferOutputStream bos(buffer);

			for (int i = 0; i < TestCount; ++i)
			{
				bos << i;
			}

			if (bos.hasError())
			{
				ls << "Unexpected error occured! Error Count = " << bos.getErrorCount() << lferr;
			}

			bos.clearErrorCount();

			bos << 0;
			bos << 0;
			bos << 0;

			if (!bos.hasError())
			{
				ls << "An error is not occured when exceeding its limit." << lferr;
			}

			if (bos.getErrorCount() != 3)
			{
				ls << "Invalid error count " << bos.getErrorCount() << ", 3 is expected." << lferr;
			}

			bos.clearErrorCount();

			if (bos.getErrorCount() != 0)
			{
				ls << "Invalid error count " << bos.getErrorCount() << ", 0 is expected." << lferr;
			}

			int* intArray = reinterpret_cast<int*>(buffer.getData());
			for (size_t i = 0; i < TestCount; ++i)
			{
				ls << i << "th value = " << intArray[i] << lf;

				if (intArray[i] != static_cast<int>(i))
				{
					ls << "Invalid value " << intArray[i] << ", " << i << " is expected." << lferr;
				}
			}
		});

		addTest("Array", [this](auto& ls)
		{
			constexpr size_t TestCount = 20;

			auto buffer = getMemoryBuffer<int>(TestCount, -1);
			BufferOutputStream bos(buffer);

			{
				int intArray[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
				bos << intArray;
			}

			if (bos.hasError())
			{
				ls << "Unexpected error occured! Error Count = " << bos.getErrorCount() << lferr;
			}

			bos.clearErrorCount();

			if (bos.getErrorCount() != 0)
			{
				ls << "Invalid error count " << bos.getErrorCount() << ", 0 is expected." << lferr;
			}

			bos.clearErrorCount();

			int* intArray = reinterpret_cast<int*>(buffer.getData() + sizeof(size_t));
			for (int i = 0; i < 10; ++i)
			{
				ls << i << "th value = " << intArray[i] << lf;

				if (intArray[i] != i)
				{
					ls << "Invalid value " << intArray[i] << ", " << i << " is expected." << lferr;
				}
			}

			if (bos.getErrorCount() != 0)
			{
				ls << "Invalid error count " << bos.getErrorCount() << ", 0 is expected." << lferr;
			}

			bos.clearErrorCount();
		});

		addTest("BufferInputStream", [this](auto& ls)
		{
			constexpr size_t TestCount = 20;

			auto buffer = getMemoryBuffer<int>(TestCount, -1);
			BufferOutputStream bos(buffer);

			{
				int intArray[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
				bos << intArray;
			}

			if (bos.hasError())
			{
				ls << "Unexpected error occured! Error Count = " << bos.getErrorCount() << lferr;
			}

			bos.clearErrorCount();

			BufferInputStream bis(buffer);

			int intArray[10];
			bis >> intArray;

			if (bos.getErrorCount() != 0)
			{
				ls << "Invalid error count " << bos.getErrorCount() << ", 0 is expected." << lferr;
			}

			bos.clearErrorCount();

			for (int i = 0; i < 10; ++i)
			{
				ls << i << " = " << intArray[i] << lf;

				if (intArray[i] != i)
				{
					ls << "Invalid value " << intArray[i] << ", " << i << " is expected." << lferr;
				}
			}
		});
	}

} // namespace hbe
#endif //__UNIT_TEST__
