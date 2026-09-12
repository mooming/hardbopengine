// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cctype>
#include <cstring>
#include <string>
#include <string_view>
#include "EndLine.h"
#include "OSAL/Intrinsic.h"
#include "StaticString.h"
#include "StringUtil.h"

namespace hbe
{

	/// @brief A fixed-size inline string builder with a static buffer for efficient string construction.
	template<size_t BufferSize = 1024, class TChar = char>
	class InlineStringBuilder final
	{
	public:
		static_assert(BufferSize > 0, "BufferSize should be greater than 0.");
		static constexpr size_t LastIndex = BufferSize - 1;
		using TThis = InlineStringBuilder;

		InlineStringBuilder() noexcept : length(0)
		{
			buffer[0] = '\0';
			buffer[LastIndex] = '\0';
		}

		~InlineStringBuilder() = default;

		void Clear() noexcept
		{
			length = 0;
			buffer[0] = '\0';
			buffer[LastIndex] = '\0';
		}

		[[nodiscard]] auto c_str() const noexcept { return static_cast<const char*>(buffer); }

		[[nodiscard]] auto Size() const noexcept { return length; }

		[[nodiscard]] operator const TChar*() const noexcept { return c_str(); }

		TThis& Hex(uint8_t value) noexcept
		{
			auto print = [&]()
			{
				const size_t remained = BufferSize - length;

				if (std::isprint(value))
				{
					return snprintf(&buffer[length], remained, "%c", value);
				}

				return snprintf(&buffer[length], remained, "%02x", value);
			};

			int written = print();

			Assert(written >= 0);
			length = std::min(LastIndex, length + written);

			return *this;
		}

		TThis& operator<<(nullptr_t) noexcept { return *this << "Null"; }

		TThis& operator<<(bool value) noexcept
		{
			if (value)
			{
				return *this << "True";
			}

			return *this << "False";
		}

		TThis& operator<<(char ch) noexcept
		{
			if (length >= LastIndex)
			{
				return *this;
			}

			buffer[length++] = ch;

			Assert(length <= LastIndex);
			buffer[length] = '\0';

			return *this;
		}

		TThis& operator<<(unsigned char value) noexcept
		{
			const size_t remained = BufferSize - length;
			auto written = snprintf(&buffer[length], remained, "%u", value);

			Assert(written >= 0);
			length = std::min(LastIndex, length + written);

			return *this;
		}

		TThis& operator<<(const char* str) noexcept
		{
			if (unlikely(str == nullptr))
			{
				return *this << nullptr;
			}

			auto len = StringUtil::StrLen(str);
			size_t newLength = length + len;

			if (unlikely(newLength >= LastIndex))
			{
				newLength = LastIndex;
				Assert(length <= LastIndex);
				len = LastIndex - length;
			}

			if (unlikely(len <= 0))
			{
				return *this;
			}

			memcpy((void*) (&buffer[length]), str, len);
			length = newLength;
			buffer[length] = '\0';

			return *this;
		}

		template<size_t N>
		TThis& operator<<(char str[N]) noexcept
		{
			if (N == 0)
			{
				return *this;
			}

			auto len = StringUtil::StrLen(str, N);
			size_t newLength = length + len;

			if (unlikely(newLength >= LastIndex))
			{
				newLength = LastIndex;
				Assert(length <= LastIndex);
				len = LastIndex - length;
			}

			if (unlikely(len <= 0))
			{
				return *this;
			}

			memcpy((void*) (&buffer[length]), &str[0], len);
			length = newLength;
			buffer[length] = '\0';

			return *this;
		}

		TThis& operator<<(const std::string_view& str) noexcept
		{
			size_t len = str.length();
			size_t newLength = length + len;

			if (unlikely(newLength >= LastIndex))
			{
				newLength = LastIndex;
				Assert(length <= LastIndex);
				len = LastIndex - length;
			}

			if (unlikely(len <= 0))
			{
				return *this;
			}

			memcpy((void*) (&buffer[length]), str.data(), len);

			length = newLength;
			buffer[length] = '\0';

			return *this;
		}

		template<class CharT, class Traits, class Allocator>
		TThis& operator<<(const std::basic_string<CharT, Traits, Allocator>& str) noexcept
		{
			return *this << static_cast<std::string_view>(str);
		}

		TThis& operator<<(StaticString str) noexcept { return *this << str.c_str(); }

		TThis& operator<<(short value) noexcept
		{
			const size_t remained = BufferSize - length;
			auto written = snprintf(&buffer[length], remained, "%d", value);

			Assert(written >= 0);
			length = std::min(LastIndex, length + written);
			buffer[length] = '\0';

			return *this;
		}

		TThis& operator<<(unsigned short value) noexcept
		{
			const size_t remained = BufferSize - length;
			auto written = snprintf(&buffer[length], remained, "%u", value);

			Assert(written >= 0);
			length = std::min(LastIndex, length + written);
			buffer[length] = '\0';

			return *this;
		}

		TThis& operator<<(int value) noexcept
		{
			const size_t remained = BufferSize - length;
			auto written = snprintf(&buffer[length], remained, "%d", value);

			Assert(written >= 0);
			length = std::min(LastIndex, length + written);
			buffer[length] = '\0';

			return *this;
		}

		TThis& operator<<(unsigned int value) noexcept
		{
			const size_t remained = BufferSize - length;
			auto written = snprintf(&buffer[length], remained, "%u", value);

			Assert(written >= 0);
			length = std::min(LastIndex, length + written);
			buffer[length] = '\0';

			return *this;
		}

		TThis& operator<<(long value) noexcept
		{
			const size_t remained = BufferSize - length;
			auto written = snprintf(&buffer[length], remained, "%ld", value);

			Assert(written >= 0);
			length = std::min(LastIndex, length + written);
			buffer[length] = '\0';

			return *this;
		}

		TThis& operator<<(unsigned long value) noexcept
		{
			const size_t remained = BufferSize - length;
			auto written = snprintf(&buffer[length], remained, "%lu", value);

			Assert(written >= 0);
			length = std::min(LastIndex, length + written);
			buffer[length] = '\0';

			return *this;
		}

		TThis& operator<<(long long value) noexcept
		{
			const size_t remained = BufferSize - length;
			auto written = snprintf(&buffer[length], remained, "%lld", value);

			Assert(written >= 0);
			length = std::min(LastIndex, length + written);
			buffer[length] = '\0';

			return *this;
		}

		TThis& operator<<(unsigned long long value) noexcept
		{
			const size_t remained = BufferSize - length;
			auto written = snprintf(&buffer[length], remained, "%llu", value);

			Assert(written >= 0);
			length = std::min(LastIndex, length + written);
			buffer[length] = '\0';

			return *this;
		}

		TThis& operator<<(float value) noexcept
		{
			const size_t remained = BufferSize - length;
			auto written = snprintf(&buffer[length], remained, "%f", value);

			Assert(written >= 0);
			length = std::min(LastIndex, length + written);
			buffer[length] = '\0';

			return *this;
		}

		TThis& operator<<(double value) noexcept
		{
			const size_t remained = BufferSize - length;
			auto written = snprintf(&buffer[length], remained, "%lf", value);

			Assert(written >= 0);
			length = std::min(LastIndex, length + written);
			buffer[length] = '\0';

			return *this;
		}

		TThis& operator<<(long double value) noexcept
		{
			const size_t remained = BufferSize - length;
			auto written = snprintf(&buffer[length], remained, "%Le", value);

			Assert(written >= 0);
			length = std::min(LastIndex, length + written);
			buffer[length] = '\0';

			return *this;
		}

		TThis& operator<<(void* value) noexcept
		{
			const size_t remained = BufferSize - length;
			auto written = snprintf(&buffer[length], remained, "%p", value);

			Assert(written >= 0);
			length = std::min(LastIndex, length + written);
			buffer[length] = '\0';

			return *this;
		}

		TThis& operator<<(EndLine) noexcept { return *this << '\n'; }

	private:
		size_t length;
		TChar buffer[BufferSize];
	};

} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{
	class InlineStringBuilderTest : public TestCollection
	{
	public:
		InlineStringBuilderTest() : TestCollection("InlineStringBuilderTest") {}

	protected:
		void Prepare() override;
	};
} // namespace hbe

#endif //__UNIT_TEST__
