// Created by mooming.go@gmail.com, 2017

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
		using Char = char;
		template<class T>
		using Vector = hbe::HVector<T>;
		static constexpr Index INVALID_INDEX = std::is_unsigned<Index>::value ? std::numeric_limits<Index>::max() : -1;

	private:
		Shareable<Vector<Char>> buffer;
		Index hashCode;

	public:
		String() : hashCode(0) { buffer->push_back('\0'); }

		String(String& rhs) : buffer(rhs.buffer), hashCode(rhs.hashCode) {}

		String(String&& rhs) : buffer(std::move(rhs.buffer)), hashCode(rhs.hashCode) { rhs.hashCode = 0; }

		String(const bool value);
		String(const Pointer ptr);
		String(const char letter);
		String(const unsigned char value);
		String(const short value);
		String(const unsigned short value);
		String(const int value);
		String(const unsigned int value);
		String(const long value);
		String(const unsigned long value);
		String(const long long value);
		String(const unsigned long long value);
		String(const float value);
		String(const double value);
		String(const long double value);
		String(const char* text);

		String(const std::string str) : String(str.c_str()) {}
		String(const String& string, Index startIndex, Index endIndex = INVALID_INDEX);

		String& operator=(String&& rhs)
		{
			Swap(std::move(rhs));
			return *this;
		}

		String& operator=(const char* text);
		String& operator=(const String& rhs);

		bool operator<(const String& rhs) const;
		bool operator>(const String& rhs) const { return rhs < *this; }
		bool operator<=(const String& rhs) const { return !(*this > rhs); }
		bool operator>=(const String& rhs) const { return !(*this < rhs); }

		bool operator==(const String& rhs) const;
		bool operator!=(const String& rhs) const { return !(*this == rhs); }

		bool operator==(const char* rhs) const;
		bool operator!=(const char* rhs) const { return !(*this == rhs); }

		bool operator==(std::nullptr_t) const { return buffer->empty(); }
		bool operator!=(std::nullptr_t) const { return !buffer->empty(); }

		String operator+(const String& str) const { return Append(str); }

		template<typename U>
		void operator+=(U str)
		{
			AppendSelf(str);
		}

		// Convenient conversion operator to (const char*)
		[[nodiscard]] operator const char*() const { return ToCharArray(); }

		[[nodiscard]] const char* c_str() const { return ToCharArray(); }

		// Return the length of this string. It doesn't count the termination letter '\0'.
		[[nodiscard]] Index Length() const { return static_cast<bool>(buffer) && buffer->size() > 0 ? buffer->size() - 1 : 0; }

		// True if it has at least one valid character. (The null termination letter will not be counted.)
		[[nodiscard]] bool IsEmpty() const { return Length() == 0; }

		[[nodiscard]] Index HashCode() const { return hashCode; }

		[[nodiscard]] String Clone() const;

		[[nodiscard]] String SubString(Index startIndex, Index endIndex = INVALID_INDEX) const
		{
			return String {*this, startIndex, endIndex};
		}

		[[nodiscard]] bool ContainsAt(const String& keyword, Index startIndex) const;

		[[nodiscard]] Index Find(const Char ch) const;
		[[nodiscard]] Index Find(const Array<Char>& chs) const;
		[[nodiscard]] Index Find(const String& keyword) const;
		[[nodiscard]] Index Find(const String& keyword, Index startIndex, Index endIndex = INVALID_INDEX) const;

		[[nodiscard]] Index FindLast(const Char ch) const;

		[[nodiscard]] bool IsValidIndex(Index index) const { return index >= 0 && index < Length(); }

		[[nodiscard]] bool StartsWith(const Char ch) const
		{
			if (IsEmpty())
			{
				return false;
			}
			return (*buffer)[0] == ch;
		}

		[[nodiscard]] bool StartsWith(const String& header) const { return ContainsAt(header, 0); }

		[[nodiscard]] bool EndsWith(const Char ch) const
		{
			if (IsEmpty())
			{
				return false;
			}

			return (*buffer)[Length() - 1] == ch;
		}

		[[nodiscard]] bool EndsWith(const String& tail) const
		{
			if (Length() < tail.Length())
			{
				return false;
			}

			return ContainsAt(tail, Length() - tail.Length());
		}

		[[nodiscard]] bool Contains(const String& keyword) const { return Find(keyword) < Length(); }

		[[nodiscard]] String Append(const Char letter) const;
		[[nodiscard]] String Append(const int value) const;
		[[nodiscard]] String Append(const float value) const;
		[[nodiscard]] String Append(const Char* text) const;
		[[nodiscard]] String Append(const String& string) const;

		void AppendSelf(const Char letter);
		void AppendSelf(const int value);
		void AppendSelf(const float value);
		void AppendSelf(const Char* text);
		void AppendSelf(const String& string);

		[[nodiscard]] String Replace(const String& from, const String& to, Index offset = 0, Index endIndex = INVALID_INDEX) const;
		[[nodiscard]] String ReplaceAll(char from, char to) const;
		[[nodiscard]] String ReplaceAll(String from, String to) const;

		[[nodiscard]] String Trim() const
		{
			Index startIndex = INVALID_INDEX;
			Index endIndex = INVALID_INDEX;

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

		[[nodiscard]] String Head() const
		{
			Index index = Find(Array<Char>({' ', '\t', '\n', '\r'}));
			if (index < Length())
			{
				return SubString(0, index);
			}

			return Clone();
		}

		[[nodiscard]] String ExceptHead() const
		{
			Index index = Find(Array<Char>({' ', '\t', '\n', '\r'}));
			if (index < Length())
			{
				return SubString(index);
			}
			return {};
		}

		[[nodiscard]] Char* GetBuffer() { return buffer->data(); }
		[[nodiscard]] const Char* GetBuffer() const { return buffer->data(); }
		void ResetBuffer(size_t size);

		void Swap(String&& target)
		{
			Index tmpHashCode = hashCode;
			hashCode = target.hashCode;
			target.hashCode = tmpHashCode;

			buffer.Swap(target.buffer);
		}

		void ToLowerCase()
		{
			constexpr Char diff = 'a' - 'A';

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

		[[nodiscard]] String GetLowerCase() const
		{
			String str = Clone();
			str.ToLowerCase();
			return str;
		}

		void ToUpperCase()
		{
			constexpr Char diff = 'A' - 'a';

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

		[[nodiscard]] String GetUpperCase() const
		{
			String str = Clone();
			str.ToUpperCase();
			return str;
		}

		[[nodiscard]] const char* ToCharArray() const;

		[[nodiscard]] char ToChar() const { return buffer ? (*buffer)[0] : '\0'; }

		[[nodiscard]] char ToUnsignedChar() const { return buffer ? static_cast<unsigned char>((*buffer)[0]) : 0; }

		[[nodiscard]] int ToInt() const { return buffer ? std::stoi(buffer->data()) : 0; }

		[[nodiscard]] unsigned int ToUnsignedInt() const
		{
			return buffer ? static_cast<unsigned int>(std::stoul(buffer->data())) : 0;
		}

		[[nodiscard]] long ToLong() const { return buffer ? std::stol(buffer->data()) : 0; }

		[[nodiscard]] unsigned long ToUnsignedLong() const { return buffer ? std::stoul(buffer->data()) : 0; }

		[[nodiscard]] long long ToLongLong() const { return buffer ? std::stoll(buffer->data()) : 0; }

		[[nodiscard]] unsigned long long ToUnsignedLongLong() const { return buffer ? std::stoull(buffer->data()) : 0; }

		[[nodiscard]] float ToFloat() const { return buffer ? std::stof(buffer->data()) : 0.0f; }

		[[nodiscard]] double ToDouble() const { return buffer ? std::stod(buffer->data()) : 0.0; }

		[[nodiscard]] long double ToLongDouble() const { return buffer ? std::stold(buffer->data()) : 0.0; }

		[[nodiscard]] void* ToPointer() const
		{
			if (buffer)
			{
				unsigned long long address = std::stoull(buffer->data(), 0, 16);
				return reinterpret_cast<void*>(address);
			}
			return nullptr;
		}

		// A simple helper function to parse 'Key = Value' string.
		void ParseKeyValue(String& outKey, String& outValue);

	private:
		void CalculateHashCode();
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
