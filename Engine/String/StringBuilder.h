// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstdio>
#include <string>
#include <string_view>
#include "EndLine.h"
#include "Memory/DefaultAllocator.h"
#include "Memory/InlinePoolAllocator.h"
#include "StaticString.h"

namespace hbe
{

	/// @brief A template-based string builder supporting various types with stream-style output.
	template<class TCh = char, class TAlloc = DefaultAllocator<TCh>>
	class StringBuilder final
	{
	public:
		static constexpr int InlineBufferSize = 32;
		static constexpr int InlineFloatBufferSize = 64;
		static constexpr int InlineLongDoubleBufferSize = 512;

		using TThis = StringBuilder;
		using TString = std::basic_string<TCh, std::char_traits<TCh>, TAlloc>;

		StringBuilder() = default;
		~StringBuilder() = default;

		void Reserve(size_t size) noexcept { buffer.reserve(size); }

		void Clear() noexcept { buffer.clear(); }

		[[nodiscard]] auto c_str() const noexcept { return buffer.c_str(); }

		[[nodiscard]] auto Size() const noexcept { return buffer.size(); }

		[[nodiscard]] operator const TCh*() const noexcept { return buffer.c_str(); }

		TThis& operator<<(nullptr_t) noexcept
		{
			buffer.append("Null");
			return *this;
		}

		TThis& operator<<(bool value) noexcept
		{
			buffer.append(value ? "True" : "False");
			return *this;
		}

		TThis& operator<<(char ch) noexcept
		{
			buffer.push_back(ch);
			return *this;
		}

		TThis& operator<<(unsigned char value) noexcept
		{
			char temp[InlineBufferSize];
			snprintf(temp, InlineBufferSize, "%u", value);
			buffer.append(temp);
			return *this;
		}

		TThis& operator<<(const char* str) noexcept
		{
			if (str == nullptr)
			{
				buffer.append("Null");
				return *this;
			}

			buffer.append(str);
			return *this;
		}

		TThis& operator<<(StaticString str) noexcept
		{
			buffer.append(str.c_str());
			return *this;
		}

		TThis& operator<<(const std::string_view& str) noexcept
		{
			buffer.append(str);

			return *this;
		}

		template<class CharT, class Traits, class Allocator>
		TThis& operator<<(const std::basic_string<CharT, Traits, Allocator>& str) noexcept
		{
			return *this << static_cast<std::string_view>(str);
		}

		TThis& operator<<(short value) noexcept
		{
			char temp[InlineBufferSize];
			snprintf(temp, InlineBufferSize, "%d", value);
			buffer.append(temp);
			return *this;
		}

		TThis& operator<<(unsigned short value) noexcept
		{
			char temp[InlineBufferSize];
			snprintf(temp, InlineBufferSize, "%u", value);
			buffer.append(temp);
			return *this;
		}

		TThis& operator<<(int value) noexcept
		{
			char temp[InlineBufferSize];
			snprintf(temp, InlineBufferSize, "%d", value);
			buffer.append(temp);
			return *this;
		}

		TThis& operator<<(unsigned int value) noexcept
		{
			char temp[InlineBufferSize];
			snprintf(temp, InlineBufferSize, "%u", value);
			buffer.append(temp);
			return *this;
		}

		TThis& operator<<(long value) noexcept
		{
			char temp[InlineBufferSize];
			snprintf(temp, InlineBufferSize, "%ld", value);
			buffer.append(temp);
			return *this;
		}

		TThis& operator<<(unsigned long value) noexcept
		{
			char temp[InlineBufferSize];
			snprintf(temp, InlineBufferSize, "%lu", value);
			buffer.append(temp);
			return *this;
		}

		TThis& operator<<(long long value) noexcept
		{
			char temp[InlineBufferSize];
			snprintf(temp, InlineBufferSize, "%lld", value);
			buffer.append(temp);
			return *this;
		}

		TThis& operator<<(unsigned long long value) noexcept
		{
			char temp[InlineBufferSize];
			snprintf(temp, InlineBufferSize, "%llu", value);
			buffer.append(temp);
			return *this;
		}

		TThis& operator<<(float value) noexcept
		{
			char temp[InlineFloatBufferSize];
			snprintf(temp, InlineFloatBufferSize, "%f", value);
			buffer.append(temp);
			return *this;
		}

		TThis& operator<<(double value) noexcept
		{
			char temp[InlineLongDoubleBufferSize];
			snprintf(temp, InlineLongDoubleBufferSize, "%lf", value);
			buffer.append(temp);
			return *this;
		}

		TThis& operator<<(long double value) noexcept
		{
			char temp[InlineLongDoubleBufferSize];
			snprintf(temp, InlineLongDoubleBufferSize, "%Le", value);
			buffer.append(temp);
			return *this;
		}

		TThis& operator<<(void* value) noexcept
		{
			char temp[InlineBufferSize];
			snprintf(temp, InlineBufferSize, "%p", value);
			buffer.append(temp);
			return *this;
		}

		TThis& operator<<(EndLine) noexcept
		{
			buffer.append("\n");
			return *this;
		}

	private:
		TString buffer;
	};

} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{
	class StringBuilderTest : public TestCollection
	{
	public:
		StringBuilderTest() : TestCollection("StringBuilderTest") {}

	protected:
		void Prepare() override;
	};
} // namespace hbe
#endif //__UNIT_TEST__
