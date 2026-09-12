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

		String operator+(const String& str) const noexcept { return Append(str); }

		template<typename U>
		void operator+=(U str) noexcept
		{
			AppendSelf(str);
		}

		[[nodiscard]] operator const char*() const noexcept { return ToCharArray(); }

		[[nodiscard]] const char* c_str() const noexcept { return ToCharArray(); }

		[[nodiscard]] Index Length() const noexcept { return static_cast<bool>(buffer) && buffer->size() > 0 ? buffer->size() - 1 : 0; }

		[[nodiscard]] bool IsEmpty() const noexcept { return Length() == 0; }

		[[nodiscard]] Index HashCode() const noexcept { return hashCode; }

		[[nodiscard]] String Clone() const noexcept;

		[[nodiscard]] String SubString(Index startIndex, Index endIndex = InvalidIndex) const noexcept
		{
			return String {*this, startIndex, endIndex};
		}

		[[nodiscard]] bool ContainsAt(const String& keyword, Index startIndex) const noexcept;

		[[nodiscard]] Index Find(const TChar ch) const noexcept;
		[[nodiscard]] Index Find(const Array<TChar>& chs) const noexcept;
		[[nodiscard]] Index Find(const String& keyword) const noexcept;
		[[nodiscard]] Index Find(const String& keyword, Index startIndex, Index endIndex = InvalidIndex) const noexcept;

		[[nodiscard]] Index FindLast(const TChar ch) const noexcept;

		[[nodiscard]] bool IsValidIndex(Index index) const noexcept { return index >= 0 && index < Length(); }

		[[nodiscard]] bool StartsWith(const TChar ch) const noexcept
		{
			if (IsEmpty())
				return false;

			return (*buffer)[0] == ch;
		}

		[[nodiscard]] bool StartsWith(const String& header) const noexcept { return ContainsAt(header, 0); }

		[[nodiscard]] bool EndsWith(const TChar ch) const noexcept
		{
			if (IsEmpty())
				return false;

			return (*buffer)[Length() - 1] == ch;
		}

		[[nodiscard]] bool EndsWith(const String& tail) const noexcept
		{
			if (Length() < tail.Length())
				return false;

			return ContainsAt(tail, Length() - tail.Length());
		}

		[[nodiscard]] bool Contains(const String& keyword) const noexcept { return Find(keyword) < Length(); }

		[[nodiscard]] String Append(const TChar letter) const noexcept;
		[[nodiscard]] String Append(const int value) const noexcept;
		[[nodiscard]] String Append(const float value) const noexcept;
		[[nodiscard]] String Append(const TChar* text) const noexcept;
		[[nodiscard]] String Append(const String& string) const noexcept;

		void AppendSelf(const TChar letter) noexcept;
		void AppendSelf(const int value) noexcept;
		void AppendSelf(const float value) noexcept;
		void AppendSelf(const TChar* text) noexcept;
		void AppendSelf(const String& string) noexcept;

		[[nodiscard]] String Replace(const String& from, const String& to, Index offset = 0, Index endIndex = InvalidIndex) const noexcept;
		[[nodiscard]] String ReplaceAll(char from, char to) const noexcept;
		[[nodiscard]] String ReplaceAll(String from, String to) const noexcept;

		[[nodiscard]] String Trim() const noexcept
		{
			Index startIndex = InvalidIndex;
			Index endIndex = InvalidIndex;

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
			{
				return {};
			}

			return SubString(startIndex, endIndex + 1);
		}

		[[nodiscard]] String Head() const noexcept
		{
			Index index = Find(Array<TChar>({' ', '\t', '\n', '\r'}));
			if (index < Length())
			{
				return SubString(0, index);
			}

			return Clone();
		}

		[[nodiscard]] String ExceptHead() const noexcept
		{
			Index index = Find(Array<TChar>({' ', '\t', '\n', '\r'}));
			if (index < Length())
			{
				return SubString(index);
			}

			return {};
		}

		[[nodiscard]] TChar* GetBuffer() noexcept { return buffer->data(); }
		[[nodiscard]] const TChar* GetBuffer() const noexcept { return buffer->data(); }
		void ResetBuffer(size_t size) noexcept;

		void Swap(String&& target) noexcept
		{
			Index tmpHashCode = hashCode;
			hashCode = target.hashCode;
			target.hashCode = tmpHashCode;

			buffer.Swap(target.buffer);
		}

		void ToLowerCase() noexcept
		{
			constexpr TChar diff = 'a' - 'A';

			auto tmp = GetBuffer();
			const auto length = Length();
			for (Index i = 0; i < length; ++i)
			{
				if (Letter::IsUpperCase(tmp[i]))
				{
					tmp[i] += diff;
				}
			}

			CalculateHashCode();
		}

		[[nodiscard]] String GetLowerCase() const noexcept
		{
			String str = Clone();
			str.ToLowerCase();

			return str;
		}

		void ToUpperCase() noexcept
		{
			constexpr TChar diff = 'A' - 'a';

			auto tmp = GetBuffer();
			const auto length = Length();
			for (Index i = 0; i < length; ++i)
			{
				if (Letter::IsLowerCase(tmp[i]))
				{
					tmp[i] += diff;
				}
			}

			CalculateHashCode();
		}

		[[nodiscard]] String GetUpperCase() const noexcept
		{
			String str = Clone();
			str.ToUpperCase();

			return str;
		}

		[[nodiscard]] const char* ToCharArray() const noexcept;

		[[nodiscard]] char ToChar() const noexcept { return buffer ? (*buffer)[0] : '\0'; }

		[[nodiscard]] char ToUnsignedChar() const noexcept { return buffer ? static_cast<unsigned char>((*buffer)[0]) : 0; }

		[[nodiscard]] int ToInt() const noexcept { return buffer ? std::stoi(buffer->data()) : 0; }

		[[nodiscard]] unsigned int ToUnsignedInt() const noexcept
		{
			return buffer ? static_cast<unsigned int>(std::stoul(buffer->data())) : 0;
		}

		[[nodiscard]] long ToLong() const noexcept { return buffer ? std::stol(buffer->data()) : 0; }

		[[nodiscard]] unsigned long ToUnsignedLong() const noexcept { return buffer ? std::stoul(buffer->data()) : 0; }

		[[nodiscard]] long long ToLongLong() const noexcept { return buffer ? std::stoll(buffer->data()) : 0; }

		[[nodiscard]] unsigned long long ToUnsignedLongLong() const noexcept { return buffer ? std::stoull(buffer->data()) : 0; }

		[[nodiscard]] float ToFloat() const noexcept { return buffer ? std::stof(buffer->data()) : 0.0f; }

		[[nodiscard]] double ToDouble() const noexcept { return buffer ? std::stod(buffer->data()) : 0.0; }

		[[nodiscard]] long double ToLongDouble() const noexcept { return buffer ? std::stold(buffer->data()) : 0.0; }

		[[nodiscard]] void* ToPointer() const noexcept
		{
			if (buffer)
			{
				unsigned long long address = std::stoull(buffer->data(), 0, 16);
				return reinterpret_cast<void*>(address);
			}

			return nullptr;
		}

		void ParseKeyValue(String& outKey, String& outValue) noexcept;

	private:
		Shareable<Vector<TChar>> buffer;
		Index hashCode;
		void CalculateHashCode() noexcept;
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
		void Prepare() override;
	};
} // namespace hbe
#endif //__UNIT_TEST__
