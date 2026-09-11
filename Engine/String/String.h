// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "Container/Array.h"
#include "Core/Types.h"
#include "HSTL/HVector.h"
#include "Letter.h"
#include "Memory/Shareable.h"

namespace hbe
{

	/// @brief A dynamic string class with automatic memory management and various utility methods.
	class String
	{
	public:
		using TChar = char;
		template<class T>
		using Vector = hbe::HVector<T>;
		static constexpr Index InvalidIndex = std::is_unsigned<Index>::value ? std::numeric_limits<Index>::max() : -1;

		String() noexcept : hashCode(0) { buffer->push_back('\0'); }

		String(String& rhs) noexcept : buffer(rhs.buffer), hashCode(rhs.hashCode) {}

		String(String&& rhs) noexcept : buffer(std::move(rhs.buffer)), hashCode(rhs.hashCode) { rhs.hashCode = 0; }

		explicit String(const bool value) noexcept;
		explicit String(const Pointer ptr) noexcept;
		explicit String(const char letter) noexcept;
		explicit String(const unsigned char value) noexcept;
		explicit String(const short value) noexcept;
		explicit String(const unsigned short value) noexcept;
		explicit String(const int value) noexcept;
		explicit String(const unsigned int value) noexcept;
		explicit String(const long value) noexcept;
		explicit String(const unsigned long value) noexcept;
		explicit String(const long long value) noexcept;
		explicit String(const unsigned long long value) noexcept;
		explicit String(const float value) noexcept;
		explicit String(const double value) noexcept;
		explicit String(const long double value) noexcept;
		String(const char* text) noexcept;
		explicit String(const std::string str) noexcept : String(str.c_str()) {}
		explicit String(const String& string, Index startIndex, Index endIndex = InvalidIndex) noexcept;

		String& operator=(String&& rhs) noexcept
		{
			Swap(std::move(rhs));
			return *this;
		}

		String& operator=(const char* text) noexcept;
		String& operator=(const String& rhs) noexcept;

		bool operator<(const String& rhs) const noexcept;
		bool operator>(const String& rhs) const noexcept { return rhs < *this; }
		bool operator<=(const String& rhs) const noexcept { return !(*this > rhs); }
		bool operator>=(const String& rhs) const noexcept { return !(*this < rhs); }

		bool operator==(const String& rhs) const noexcept;
		bool operator!=(const String& rhs) const noexcept { return !(*this == rhs); }

		bool operator==(const char* rhs) const noexcept;
		bool operator!=(const char* rhs) const noexcept { return !(*this == rhs); }

		bool operator==(std::nullptr_t) const noexcept { return buffer->empty(); }
		bool operator!=(std::nullptr_t) const noexcept { return !buffer->empty(); }

		String operator+(const String& str) const noexcept { return append(str); }

		template<typename U>
		void operator+=(U str) noexcept
		{
			appendSelf(str);
		}

		[[nodiscard]] operator const char*() const noexcept { return toCharArray(); }

		[[nodiscard]] const char* c_str() const noexcept { return toCharArray(); }

		[[nodiscard]] Index Length() const noexcept { return static_cast<bool>(buffer) && buffer->size() > 0 ? buffer->size() - 1 : 0; }

		[[nodiscard]] bool IsEmpty() const noexcept { return Length() == 0; }

		[[nodiscard]] Index HashCode() const noexcept { return hashCode; }

		[[nodiscard]] String clone() const noexcept;

		[[nodiscard]] String subString(Index startIndex, Index endIndex = InvalidIndex) const noexcept
		{
			return String {*this, startIndex, endIndex};
		}

		[[nodiscard]] bool containsAt(const String& keyword, Index startIndex) const noexcept;

		[[nodiscard]] Index find(const TChar ch) const noexcept;
		[[nodiscard]] Index find(const Array<TChar>& chs) const noexcept;
		[[nodiscard]] Index find(const String& keyword) const noexcept;
		[[nodiscard]] Index find(const String& keyword, Index startIndex, Index endIndex = InvalidIndex) const noexcept;

		[[nodiscard]] Index findLast(const TChar ch) const noexcept;

		[[nodiscard]] bool isValidIndex(Index index) const noexcept { return index >= 0 && index < Length(); }

		[[nodiscard]] bool startsWith(const TChar ch) const noexcept
		{
			if (IsEmpty())
				return false;

			return (*buffer)[0] == ch;
		}

		[[nodiscard]] bool startsWith(const String& header) const noexcept { return containsAt(header, 0); }

		[[nodiscard]] bool endsWith(const TChar ch) const noexcept
		{
			if (IsEmpty())
				return false;

			return (*buffer)[Length() - 1] == ch;
		}

		[[nodiscard]] bool endsWith(const String& tail) const noexcept
		{
			if (Length() < tail.Length())
				return false;

			return containsAt(tail, Length() - tail.Length());
		}

		[[nodiscard]] bool contains(const String& keyword) const noexcept { return find(keyword) < Length(); }

		[[nodiscard]] String append(const TChar letter) const noexcept;
		[[nodiscard]] String append(const int value) const noexcept;
		[[nodiscard]] String append(const float value) const noexcept;
		[[nodiscard]] String append(const TChar* text) const noexcept;
		[[nodiscard]] String append(const String& string) const noexcept;

		void appendSelf(const TChar letter) noexcept;
		void appendSelf(const int value) noexcept;
		void appendSelf(const float value) noexcept;
		void appendSelf(const TChar* text) noexcept;
		void appendSelf(const String& string) noexcept;

		[[nodiscard]] String replace(const String& from, const String& to, Index offset = 0, Index endIndex = InvalidIndex) const noexcept;
		[[nodiscard]] String replaceAll(char from, char to) const noexcept;
		[[nodiscard]] String replaceAll(String from, String to) const noexcept;

		[[nodiscard]] String trim() const noexcept
		{
			Index startIndex = InvalidIndex;
			Index endIndex = InvalidIndex;

			const auto length = Length();
			for (Index i = 0; i < length; ++i)
			{
				if (Letter::isGenuineLetter((*buffer)[i]))
				{
					if (startIndex >= length)
					{
						startIndex = i;
					}
					endIndex = i;
				}
			}

			if (startIndex >= length)
			{
				return {};
			}

			return subString(startIndex, endIndex + 1);
		}

		[[nodiscard]] String head() const noexcept
		{
			Index index = find(Array<TChar>({' ', '\t', '\n', '\r'}));
			if (index < Length())
			{
				return subString(0, index);
			}

			return clone();
		}

		[[nodiscard]] String exceptHead() const noexcept
		{
			Index index = find(Array<TChar>({' ', '\t', '\n', '\r'}));
			if (index < Length())
			{
				return subString(index);
			}

			return {};
		}

		[[nodiscard]] TChar* getBuffer() noexcept { return buffer->data(); }
		[[nodiscard]] const TChar* getBuffer() const noexcept { return buffer->data(); }
		void resetBuffer(size_t size) noexcept;

		void Swap(String&& target) noexcept
		{
			Index tmpHashCode = hashCode;
			hashCode = target.hashCode;
			target.hashCode = tmpHashCode;

			buffer.Swap(target.buffer);
		}

		void toLowerCase() noexcept
		{
			constexpr TChar diff = 'a' - 'A';

			auto tmp = getBuffer();
			const auto length = Length();
			for (Index i = 0; i < length; ++i)
			{
				if (Letter::isUpperCase(tmp[i]))
				{
					tmp[i] += diff;
				}
			}

			calculateHashCode();
		}

		[[nodiscard]] String getLowerCase() const noexcept
		{
			String str = clone();
			str.toLowerCase();

			return str;
		}

		void toUpperCase() noexcept
		{
			constexpr TChar diff = 'A' - 'a';

			auto tmp = getBuffer();
			const auto length = Length();
			for (Index i = 0; i < length; ++i)
			{
				if (Letter::isLowerCase(tmp[i]))
				{
					tmp[i] += diff;
				}
			}

			calculateHashCode();
		}

		[[nodiscard]] String getUpperCase() const noexcept
		{
			String str = clone();
			str.toUpperCase();

			return str;
		}

		[[nodiscard]] const char* toCharArray() const noexcept;

		[[nodiscard]] char toChar() const noexcept { return buffer ? (*buffer)[0] : '\0'; }

		[[nodiscard]] char toUnsignedChar() const noexcept { return buffer ? static_cast<unsigned char>((*buffer)[0]) : 0; }

		[[nodiscard]] int toInt() const noexcept { return buffer ? std::stoi(buffer->data()) : 0; }

		[[nodiscard]] unsigned int toUnsignedInt() const noexcept
		{
			return buffer ? static_cast<unsigned int>(std::stoul(buffer->data())) : 0;
		}

		[[nodiscard]] long toLong() const noexcept { return buffer ? std::stol(buffer->data()) : 0; }

		[[nodiscard]] unsigned long toUnsignedLong() const noexcept { return buffer ? std::stoul(buffer->data()) : 0; }

		[[nodiscard]] long long toLongLong() const noexcept { return buffer ? std::stoll(buffer->data()) : 0; }

		[[nodiscard]] unsigned long long toUnsignedLongLong() const noexcept { return buffer ? std::stoull(buffer->data()) : 0; }

		[[nodiscard]] float toFloat() const noexcept { return buffer ? std::stof(buffer->data()) : 0.0f; }

		[[nodiscard]] double toDouble() const noexcept { return buffer ? std::stod(buffer->data()) : 0.0; }

		[[nodiscard]] long double toLongDouble() const noexcept { return buffer ? std::stold(buffer->data()) : 0.0; }

		[[nodiscard]] void* toPointer() const noexcept
		{
			if (buffer)
			{
				unsigned long long address = std::stoull(buffer->data(), 0, 16);
				return reinterpret_cast<void*>(address);
			}

			return nullptr;
		}

		void parseKeyValue(String& outKey, String& outValue) noexcept;

	private:
		Shareable<Vector<TChar>> buffer;
		Index hashCode;
		void calculateHashCode() noexcept;
	};
} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{
	class StringTest : public TestCollection
	{
	public:
		StringTest() : TestCollection("StringTest") {}

	protected:
		void prepare() override;
	};
} // namespace hbe
#endif //__UNIT_TEST__
