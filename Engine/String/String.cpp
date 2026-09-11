// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "String.h"

#include <cstdio>
#include <cstring>
#include "Core/Debug.h"
#include "StringUtil.h"


namespace hbe
{

	String::String(const bool value) noexcept : hashCode(0)
	{
		if (value)
		{
			buffer->resize(5);

			auto& text = *buffer;
			text[0] = 't';
			text[1] = 'r';
			text[2] = 'u';
			text[3] = 'e';
			text[4] = '\0';
		}
		else
		{
			buffer->resize(6);

			auto& text = *buffer;
			text[0] = 'f';
			text[1] = 'a';
			text[2] = 'l';
			text[3] = 's';
			text[4] = 'e';
			text[5] = '\0';
		}

		calculateHashCode();
	}

	String::String(const Pointer ptr) noexcept : hashCode(0)
	{
		buffer->resize(32);
		auto& text = *buffer;
		snprintf(text.data(), text.size(), "%p", ptr);
		calculateHashCode();
	}

	String::String(const char letter) noexcept : hashCode(0)
	{
		buffer->resize(2);
		auto& text = *buffer;
		text[0] = letter;
		text[1] = '\0';

		calculateHashCode();
	}

	String::String(const unsigned char value) noexcept : hashCode(0)
	{
		buffer->resize(16);
		auto& text = *buffer;
		snprintf(text.data(), text.size(), "0x%02X", value);
		buffer->resize(strlen(text.data()) + 1);

		calculateHashCode();
	}

	String::String(const short value) noexcept : hashCode(0)
	{
		buffer->resize(16);
		auto& text = *buffer;
		snprintf(text.data(), text.size(), "%d", value);
		buffer->resize(StringUtil::strLen(text.data()) + 1);

		calculateHashCode();
	}

	String::String(const unsigned short value) noexcept : hashCode(0)
	{
		buffer->resize(16);
		auto& text = *buffer;
		snprintf(text.data(), text.size(), "%u", value);
		buffer->resize(StringUtil::strLen(text.data()) + 1);

		calculateHashCode();
	}

	String::String(const int value) noexcept : hashCode(0)
	{
		buffer->resize(16);
		auto& text = *buffer;
		snprintf(text.data(), text.capacity(), "%d", value);
		buffer->resize(StringUtil::strLen(text.data()) + 1);

		calculateHashCode();
	}

	String::String(const unsigned int value) noexcept : hashCode(0)
	{
		buffer->resize(16);
		auto& text = *buffer;
		snprintf(text.data(), text.size(), "%u", value);
		buffer->resize(StringUtil::strLen(text.data()) + 1);

		calculateHashCode();
	}

	String::String(const long value) noexcept : hashCode(0)
	{
		buffer->resize(16);
		auto& text = *buffer;
		snprintf(text.data(), text.size(), "%ld", value);
		buffer->resize(StringUtil::strLen(text.data()) + 1);

		calculateHashCode();
	}

	String::String(const unsigned long value) noexcept : hashCode(0)
	{
		buffer->resize(16);
		auto& text = *buffer;
		snprintf(text.data(), text.size(), "%lu", value);
		buffer->resize(StringUtil::strLen(text.data()) + 1);

		calculateHashCode();
	}

	String::String(const long long value) noexcept : hashCode(0)
	{
		buffer->resize(16);
		auto& text = *buffer;
		snprintf(text.data(), text.size(), "%lld", value);
		buffer->resize(StringUtil::strLen(text.data()) + 1);

		calculateHashCode();
	}

	String::String(const unsigned long long value) noexcept : hashCode(0)
	{
		buffer->resize(16);
		auto& text = *buffer;
		snprintf(text.data(), text.size(), "%llu", value);
		buffer->resize(StringUtil::strLen(text.data()) + 1);

		calculateHashCode();
	}

	String::String(const float value) noexcept : hashCode(0)
	{
		buffer->resize(16);
		auto& text = *buffer;
		snprintf(text.data(), text.size(), "%f", value);
		buffer->resize(StringUtil::strLen(text.data()) + 1);

		calculateHashCode();
	}

	String::String(const double value) noexcept : hashCode(0)
	{
		buffer->resize(16);
		auto& text = *buffer;
		snprintf(text.data(), text.size(), "%f", value);
		buffer->resize(StringUtil::strLen(text.data()) + 1);

		calculateHashCode();
	}

	String::String(const long double value) noexcept : hashCode(0)
	{
		buffer->resize(16);
		auto& text = *buffer;
		snprintf(text.data(), text.size(), "%Lf", value);
		buffer->resize(StringUtil::strLen(text.data()) + 1);

		calculateHashCode();
	}

	String::String(const char* text) noexcept : hashCode(0)
	{
		if (text == nullptr)
		{
			text = "";
		}

		const auto totalLength = strlen(text) + 1;
		buffer->resize(totalLength);
		Vector<char>& textVec = *buffer;
		memcpy(textVec.data(), text, totalLength);

		calculateHashCode();
	}

	String::String(const String& string, Index startIndex, Index endIndex) noexcept : buffer()
	{
		if (startIndex >= string.Length())
		{
			startIndex = string.Length();
		}

		if (endIndex > string.Length())
		{
			endIndex = string.Length();
		}

		if (startIndex > endIndex)
		{
			startIndex = endIndex;
		}

		auto length = endIndex - startIndex;

		if (length > 0)
		{
			buffer->resize(length + 1);

			auto ptr = buffer->data();
			memcpy(ptr, string.buffer->data() + startIndex, length);
			ptr[length] = '\0';

			calculateHashCode();
		}
		else
		{
			hashCode = 0;
		}
	}

	String& String::operator=(const char* text) noexcept
	{
		if (text == nullptr)
		{
			text = "";
		}

		if (buffer.getReferenceCount() > 1)
		{
			Swap(String(text));
		}
		else
		{
			const auto textLength = strlen(text) + 1;
			buffer->resize(textLength);
			memcpy(buffer->data(), text, textLength);
		}

		return *this;
	}

	String& String::operator=(const String& rhs) noexcept
	{
		if (buffer.getReferenceCount() > 1)
		{
			Swap(String(rhs.toCharArray()));
		}
		else
		{
			const auto length = rhs.buffer->size();
			buffer->resize(length);
			memcpy(buffer->data(), rhs.buffer->data(), length);
		}

		return *this;
	}

	bool String::operator<(const String& rhs) const noexcept
	{
		const Index shorterLen = std::min(Length(), rhs.Length());
		Index matchCount = 0;

		for (Index i = 0; i < shorterLen; ++i)
		{
			if ((*buffer)[i] == (*rhs.buffer)[i])
			{
				++matchCount;

				continue;
			}

			if ((*buffer)[i] > (*rhs.buffer)[i])
			{
				return false;
			}

			return true;
		}

		if (matchCount == shorterLen)
		{
			return Length() < rhs.Length();
		}

		return true;
	}

	bool String::operator==(const String& string) const noexcept
	{
		if (hashCode != string.hashCode)
		{
			return false;
		}

		const auto length = Length();
		if (length != string.Length())
		{
			return false;
		}

		for (Index i = 0; i < length; ++i)
		{
			if ((*buffer)[i] != (*string.buffer)[i])
			{
				return false;
			}
		}

		return true;
	}

	bool String::operator==(const char* rhs) const noexcept
	{
		const bool isEmpty = IsEmpty();
		if (rhs == nullptr)
		{
			return isEmpty;
		}

		if (isEmpty)
		{
			return rhs[0] == '\0';
		}

		const auto length = Length();
		for (Index i = 0; i < length; ++i)
		{
			if ((*buffer)[i] != rhs[i])
			{
				return false;
			}
		}

		return rhs[length] == '\0';
	}

	const char* String::toCharArray() const noexcept { return buffer ? buffer.get().data() : ""; }

	String String::clone() const noexcept
	{
		String str;
		Assert(str.buffer);

		*(str.buffer) = *buffer;

		return str;
	}

	bool String::containsAt(const String& keyword, Index startIndex) const noexcept
	{
		const Index endIndex = startIndex + keyword.Length();

		if (endIndex > Length())
		{
			return false;
		}

		Index index = 0;

		for (Index i = startIndex; i < endIndex; ++i, ++index)
		{
			if ((*buffer)[i] != (*keyword.buffer)[index])
			{
				return false;
			}
		}

		return true;
	}

	Index String::find(const TChar ch) const noexcept
	{
		const auto length = Length();

		for (Index i = 0; i < length; ++i)
		{
			if ((*buffer)[i] == ch)
			{
				return i;
			}
		}

		return length;
	}

	Index String::find(const Array<TChar>& chs) const noexcept
	{
		const auto length = Length();
		auto chsLen = chs.Size();

		for (Index i = 0; i < length; ++i)
		{
			for (decltype(chsLen) j = 0; j < chsLen; ++j)
			{
				if ((*buffer)[i] == chs[j])
				{
					return i;
				}
			}
		}

		return length;
	}

	Index String::find(const String& keyword) const noexcept
	{
		const auto length = Length();
		const auto keywordLength = keyword.Length();

		if (keywordLength > length)
		{
			return length;
		}

		const Index lastIndex = length - keywordLength + 1;
		for (Index i = 0; i < lastIndex; ++i)
		{
			if (containsAt(keyword, i))
			{
				return i;
			}
		}

		return length;
	}

	Index String::find(const String& keyword, Index startIndex, Index endIndex) const noexcept
	{
		const auto length = Length();
		const auto keywordLength = keyword.Length();

		Assert(startIndex < length);
		if (startIndex >= length)
		{
			startIndex = length - 1;
		}

		Assert(endIndex >= startIndex);
		if (endIndex < startIndex)
		{
			endIndex = startIndex;
		}

		Assert(endIndex <= length);
		if (endIndex > length)
		{
			endIndex = length;
		}

		if ((startIndex + keywordLength) > endIndex)
		{
			return length;
		}

		const Index lastIndex = endIndex - keywordLength + 1;
		for (Index i = startIndex; i < lastIndex; ++i)
		{
			if (containsAt(keyword, i))
			{
				return i;
			}
		}

		return length;
	}

	Index String::findLast(const TChar ch) const noexcept
	{
		const auto length = Length();
		for (Index i = length; i > 0;)
		{
			if ((*buffer)[--i] == ch)
			{
				return i;
			}
		}

		return length;
	}

	String String::append(const TChar letter) const noexcept
	{
		String str;
		const auto length = Length();

		str.buffer->resize(length + sizeof(TChar) + 1);
		memcpy(str.buffer->data(), buffer->data(), length);

		(*str.buffer)[length] = letter;
		(*str.buffer)[length + 1] = '\0';

		return str;
	}

	String String::append(const int value) const noexcept
	{
		char tmp[16];
		snprintf(tmp, sizeof(tmp), "%d", value);

		const auto length = Length();
		const Index tmpLength = static_cast<Index>(strlen(tmp));

		String str;
		str.buffer->resize(length + tmpLength + 1);

		memcpy(str.buffer->data(), buffer->data(), length);
		memcpy(str.buffer->data() + length, tmp, tmpLength + 1);

		return str;
	}

	String String::append(const float value) const noexcept
	{
		char tmp[16];
		snprintf(tmp, sizeof(tmp), "%f", value);

		const auto length = Length();
		const Index tmpLength = static_cast<Index>(strlen(tmp));

		String str;
		str.buffer->resize(length + tmpLength + 1);

		memcpy(str.buffer->data(), buffer->data(), length);
		memcpy(str.buffer->data() + length, tmp, tmpLength + 1);

		return str;
	}

	String String::append(const TChar* text) const noexcept
	{
		const auto length = Length();
		const Index textLength = static_cast<Index>(strlen(text));

		String str;
		str.buffer->resize(length + textLength + 1);

		memcpy(str.buffer->data(), buffer->data(), length);
		memcpy(str.buffer->data() + length, text, textLength + 1);

		return str;
	}

	String String::append(const String& string) const noexcept
	{
		if (string.IsEmpty())
		{
			return clone();
		}

		const auto length = Length();
		const auto strLength = string.Length();

		String str;
		str.buffer->resize(length + strLength + 1);

		memcpy(str.buffer->data(), buffer->data(), length);
		memcpy(str.buffer->data() + length, string.buffer->data(), strLength + 1);

		return str;
	}

	void String::appendSelf(const TChar letter) noexcept
	{
		// Optimized single-pass append: obtain a reference to the buffer once,
		// resize to accommodate the letter + trailing null, and write both
		// directly. This halves the Shareable dereferencing overhead versus
		// the previous push_back('\0') + assign-at-index pattern.
		auto& buf = buffer.get();
		const auto index = buf.size();
		buf.resize(index + 2); // room for the letter + trailing null terminator
		buf.data()[index] = letter;
		buf.data()[index + 1] = '\0';
	}

	void String::appendSelf(const int value) noexcept
	{
		char tmp[16];
		snprintf(tmp, sizeof(tmp), "%d", value);

		const auto length = Length();
		const Index tmpLength = static_cast<Index>(strlen(tmp));
		const auto newLength = length + tmpLength + 1;

		if (newLength > buffer->capacity())
		{
			buffer->reserve(newLength * 3 / 2);
		}

		buffer->resize(newLength);
		memcpy(buffer->data() + length, tmp, tmpLength + 1);
	}

	void String::appendSelf(const float value) noexcept
	{
		char tmp[16];
		snprintf(tmp, sizeof(tmp), "%f", value);

		const auto length = Length();
		const Index tmpLength = static_cast<Index>(strlen(tmp));
		const auto newLength = length + tmpLength + 1;

		if (newLength > buffer->capacity())
		{
			buffer->reserve(newLength * 3 / 2);
		}

		buffer->resize(newLength);
		memcpy(buffer->data() + length, tmp, tmpLength + 1);
	}

	void String::appendSelf(const TChar* text) noexcept
	{
		const auto length = Length();
		const Index textLength = static_cast<Index>(strlen(text));
		const auto newLength = length + textLength + 1;

		if (newLength > buffer->capacity())
		{
			buffer->reserve(newLength * 3 / 2);
		}

		buffer->resize(newLength);
		memcpy(buffer->data() + length, text, textLength + 1);
	}

	void String::appendSelf(const String& string) noexcept
	{
		if (string.IsEmpty())
		{
			return;
		}

		const auto length = Length();
		const Index textLength = string.Length();
		const auto newLength = length + textLength + 1;

		if (newLength > buffer->capacity())
		{
			buffer->reserve(newLength * 3 / 2);
		}

		buffer->resize(newLength);
		memcpy(buffer->data() + length, string.buffer->data(), textLength + 1);
	}

	String String::replace(const String& from, const String& to, Index offset, Index endIndex) const noexcept
	{
		if (!buffer || from.IsEmpty())
		{
			return clone();
		}

		const Index strLength = Length();
		const Index actualEndIndex =  !isValidIndex(endIndex) ? strLength : endIndex;
		const Index actualOffset = !isValidIndex(offset) ? 0 : offset;
		if (actualOffset >= actualEndIndex)
		{
			return clone();
		}

		const Index searchLength = from.Length();
		Index foundIndex = strLength;

		for (Index i = actualOffset; i <= actualEndIndex - searchLength; ++i)
		{
			bool match = true;
			for (Index j = 0; j < searchLength; ++j)
			{
				if ((*buffer)[i + j] != from.buffer->data()[j])
				{
					match = false;
					break;
				}
			}

			if (match)
			{
				foundIndex = i;
				break;
			}
		}

		if (!isValidIndex(foundIndex))
		{
			return clone();
		}

		String result;
		result.buffer->clear();

		for (Index i = 0; i < foundIndex; ++i)
		{
			result.buffer->push_back((*buffer)[i]);
		}

		for (Index i = 0; i < to.Length(); ++i)
		{
			result.buffer->push_back(to.buffer->data()[i]);
		}

		for (Index i = foundIndex + searchLength; i < strLength; ++i)
		{
			result.buffer->push_back((*buffer)[i]);
		}

		result.buffer->push_back('\0');
		result.calculateHashCode();

		return result;
	}

	String String::replaceAll(char from, char to) const noexcept
	{
		if (!buffer)
		{
			return {};
		}

		String str = clone();
		TChar* data = str.buffer->data();
		Assert(data != nullptr);

		Index length = str.Length();
		for (Index i = 0; i < length; ++i)
		{
			if (data[i] == from)
			{
				data[i] = to;
			}
		}

		str.calculateHashCode();

		return str;
	}

	String String::replaceAll(String from, String to) const noexcept
	{
		if (!buffer || from.IsEmpty())
		{
			return clone();
		}

		const Index strLength = Length();
		const Index searchLength = from.Length();
		const Index toLength = to.Length();

		if (searchLength > strLength)
		{
			return clone();
		}

		String result;
		result.buffer->clear();

		Index i = 0;
		const Index lastIndex = strLength - searchLength;
		while (i <= lastIndex)
		{
			bool match = true;
			for (Index j = 0; j < searchLength; ++j)
			{
				if ((*buffer)[i + j] != from.buffer->data()[j])
				{
					match = false;
					break;
				}
			}

			if (match)
			{
				for (Index j = 0; j < toLength; ++j)
				{
					result.buffer->push_back(to.buffer->data()[j]);
				}

				i += searchLength;
			}
			else
			{
				result.buffer->push_back((*buffer)[i]);
				++i;
			}
		}

		while (i < strLength)
		{
			result.buffer->push_back((*buffer)[i]);
			++i;
		}

		result.buffer->push_back('\0');
		result.calculateHashCode();

		return result;
	}

	void String::parseKeyValue(String& key, String& value) noexcept
	{
		auto index = find('=');
		key = subString(0, index).trim();
		value = subString(index + 1).trim();
	}

	void String::calculateHashCode() noexcept
	{
		hashCode = 5381;

		const auto length = Length();
		auto text = buffer->data();

		Assert(length != 0 || text[0] == '\0');

		for (Index i = 0; i < length; ++i)
		{
			Index ch = text[i];
			hashCode = ((hashCode << 5) + hashCode) + ch; /* hash * 33 + c */
		}
	}

	void String::resetBuffer(size_t size) noexcept
	{
		buffer->reserve(static_cast<Index>(size + 1));
		buffer->clear();
		buffer->push_back('\0');
	}

} // namespace hbe

#ifdef __UNIT_TEST__
#include "Core/ScopedTime.h"

namespace hbe
{

	void StringTest::prepare()
	{
		addTest("Comparison with Zero-Terminated String", [this](auto& ls)
		{
			String str("Hello? World!");
			ls << str.c_str() << lf;

			if (str != "Hello? World!")
			{
				ls << "String Compare Failure. " << str << lferr;
			}
		});

		addTest("To Lower Case", [this](auto& ls)
		{
			String str("Hello? World!");

			auto lower = str.getLowerCase();
			ls << lower.c_str() << lf;

			if (lower != "hello? world!")
			{
				ls << "To lowercase failed. " << lower << lferr;
			}
		});

		addTest("To Upper Case", [this](auto& ls)
		{
			String str("Hello? World!");

			auto upper = str.getUpperCase();
			ls << upper.c_str() << lf;

			if (upper != "HELLO? WORLD!")
			{
				ls << "To uppercase failed. " << upper << lferr;
			}
		});

		addTest("Move Semantics", [this](auto& ls)
		{
			String str("Hello? World!");

			auto upper = str.getUpperCase();
			auto tmpString = std::move(upper);
			ls << tmpString.c_str() << lf;

			if (tmpString != "HELLO? WORLD!")
			{
				ls << "String move failed." << lferr;
			}
		});

		addTest("Find Last", [this](auto& ls)
		{
			String str("Hello? World!");
			auto lastL = str.findLast('l');
			if (lastL != 10)
			{
				ls << "Failed to find the last 'l', index = " << lastL << ", but expected 10." << lferr;
			}
		});

		addTest("SubString", [this](auto& ls)
		{
			String str("Hello? World!");
			auto lastL = str.findLast('l');
			auto afterL = str.subString(lastL);
			ls << afterL.c_str() << lf;

			if (afterL != "ld!")
			{
				ls << "Substring failed: " << afterL << lferr;
			}
		});

		addTest("Unsigned Short Constructor", [this](auto& ls)
		{
			unsigned short value = 42;
			String str(value);
			ls << "Unsigned short 42 = " << str.c_str() << lf;

			if (str != "42")
			{
				ls << "Unsigned short constructor failed: expected '42', got '" << str.c_str() << "'" << lferr;
			}

			unsigned short zero = 0;
			String strZero(zero);
			if (strZero != "0")
			{
				ls << "Unsigned short constructor failed for 0: expected '0', got '" << strZero.c_str() << "'" << lferr;
			}

			unsigned short maxVal = 65535;
			String strMax(maxVal);
			if (strMax != "65535")
			{
				ls << "Unsigned short constructor failed for 65535: expected '65535', got '" << strMax.c_str() << "'" << lferr;
			}
		});

		addTest("Replace Single", [this](auto& ls)
		{
			String str("Hello World");
			auto result = str.replace(String("World"), String("Engine"));
			ls << "Replace: " << result.c_str() << lf;

			if (result != "Hello Engine")
			{
				ls << "Replace failed: expected 'Hello Engine', got '" << result.c_str() << "'" << lferr;
			}
		});

		addTest("Replace Not Found", [this](auto& ls)
		{
			String str("Hello World");
			auto result = str.replace(String("Foo"), String("Bar"));
			ls << "Replace not found: " << result.c_str() << lf;

			if (result != "Hello World")
			{
				ls << "Replace not found should return original: expected 'Hello World', got '" << result.c_str() << "'" << lferr;
			}
		});

		addTest("Replace With Offset", [this](auto& ls)
		{
			String str("foo bar foo baz");
			auto result = str.replace(String("foo"), String("XXX"), 5);
			ls << "Replace with offset: " << result.c_str() << lf;

			if (result != "foo bar XXX baz")
			{
				ls << "Replace with offset failed: expected 'foo bar XXX baz', got '" << result.c_str() << "'" << lferr;
			}
		});

		addTest("Replace Empty From", [this](auto& ls)
		{
			String str("Hello");
			auto result = str.replace(String(""), String("X"));
			ls << "Replace empty from: " << result.c_str() << lf;

			if (result != "Hello")
			{
				ls << "Replace empty from should return clone: expected 'Hello', got '" << result.c_str() << "'" << lferr;
			}
		});

		addTest("Replace Longer To", [this](auto& ls)
		{
			String str("abc");
			auto result = str.replace(String("b"), String("XYZ"));
			ls << "Replace longer to: " << result.c_str() << lf;

			if (result != "aXYZc")
			{
				ls << "Replace longer to failed: expected 'aXYZc', got '" << result.c_str() << "'" << lferr;
			}
		});

		addTest("Replace Shorter To", [this](auto& ls)
		{
			String str("Hello World");
			auto result = str.replace(String(" World"), String(""));
			ls << "Replace shorter to: " << result.c_str() << lf;

			if (result != "Hello")
			{
				ls << "Replace shorter to failed: expected 'Hello', got '" << result.c_str() << "'" << lferr;
			}
		});

		addTest("ReplaceAll Single Char Pattern", [this](auto& ls)
		{
			String str("a.b.c.d");
			auto result = str.replaceAll(String("."), String("-"));
			ls << "ReplaceAll: " << result.c_str() << lf;

			if (result != "a-b-c-d")
			{
				ls << "ReplaceAll failed: expected 'a-b-c-d', got '" << result.c_str() << "'" << lferr;
			}
		});

		addTest("ReplaceAll Multi Char Pattern", [this](auto& ls)
		{
			String str("foo bar foo baz foo");
			auto result = str.replaceAll(String("foo"), String("XXX"));
			ls << "ReplaceAll multi: " << result.c_str() << lf;

			if (result != "XXX bar XXX baz XXX")
			{
				ls << "ReplaceAll multi failed: expected 'XXX bar XXX baz XXX', got '" << result.c_str() << "'" << lferr;
			}
		});

		addTest("ReplaceAll Not Found", [this](auto& ls)
		{
			String str("Hello World");
			auto result = str.replaceAll(String("Foo"), String("Bar"));
			ls << "ReplaceAll not found: " << result.c_str() << lf;

			if (result != "Hello World")
			{
				ls << "ReplaceAll not found should return original: expected 'Hello World', got '" << result.c_str() << "'" << lferr;
			}
		});

		addTest("ReplaceAll Empty From", [this](auto& ls)
		{
			String str("Hello");
			auto result = str.replaceAll(String(""), String("X"));
			ls << "ReplaceAll empty from: " << result.c_str() << lf;

			if (result != "Hello")
			{
				ls << "ReplaceAll empty from should return clone: expected 'Hello', got '" << result.c_str() << "'" << lferr;
			}
		});

		addTest("ReplaceAll Longer Replacement", [this](auto& ls)
		{
			String str("abc");
			auto result = str.replaceAll(String("b"), String("XYZ"));
			ls << "ReplaceAll longer: " << result.c_str() << lf;

			if (result != "aXYZc")
			{
				ls << "ReplaceAll longer failed: expected 'aXYZc', got '" << result.c_str() << "'" << lferr;
			}
		});

		addTest("ReplaceAll Shorter Replacement", [this](auto& ls)
		{
			String str("Hello World");
			auto result = str.replaceAll(String("o"), String(""));
			ls << "ReplaceAll shorter: " << result.c_str() << lf;

			if (result != "Hell Wrld")
			{
				ls << "ReplaceAll shorter failed: expected 'Hell Wrld', got '" << result.c_str() << "'" << lferr;
			}
		});

		addTest("ReplaceAll Adjacent Matches", [this](auto& ls)
		{
			String str("aabbcc");
			auto result = str.replaceAll(String("ab"), String("X"));
			ls << "ReplaceAll adjacent: " << result.c_str() << lf;

			if (result != "aXbcc")
			{
				ls << "ReplaceAll adjacent failed: expected 'aXbcc', got '" << result.c_str() << "'" << lferr;
			}
		});

		addTest("Performance", [this](auto& ls)
		{
			constexpr int COUNT = 100000;

			time::TDuration heTime;

			{
				time::ScopedTime measure(heTime);

				String str;
				for (int i = 0; i < COUNT; ++i)
				{
					str = "";
					for (char ch = 'a'; ch <= 'z'; ++ch)
					{
						str += ch;
					}
				}
			}

			time::TDuration stlTime;

			{
				time::ScopedTime measure(stlTime);

				std::string str;
				for (int i = 0; i < COUNT; ++i)
				{
					str = "";
					for (char ch = 'a'; ch <= 'z'; ++ch)
					{
						str += ch;
					}
				}
			}

			ls << "Time: he = " << time::toFloat(heTime) << ", stl = " << time::toFloat(stlTime) << lf;

			if (heTime > stlTime)
			{
				ls << "HE String is slower than STL string." << std::endl
				   << "Time: he = " << time::toFloat(heTime) << ", stl = " << time::toFloat(stlTime) << lfwarn;
			}
		});
	}

} // namespace hbe
#endif //__UNIT_TEST__
