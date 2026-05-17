// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstddef>
#include <cstdint>
#include <thread>

#include "Buffer.h"
#include "Core/Debug.h"
#include "HSTL/HString.h"

namespace hbe
{

	/// @brief Stream for writing primitive types and strings into a buffer.
	class BufferOutputStream final
	{
	public:
		using This = BufferOutputStream;

		BufferOutputStream(const BufferOutputStream&) = delete;
		BufferOutputStream(BufferOutputStream&&) = delete;

		explicit BufferOutputStream(Buffer& buffer) noexcept;
		~BufferOutputStream() = default;

		[[nodiscard]] auto GetCursor() const noexcept { return cursor; }
		[[nodiscard]] auto GetErrorCount() const noexcept { return errorCount; }
		[[nodiscard]] bool HasError() const noexcept { return errorCount > 0; }
		void ClearErrorCount() noexcept { errorCount = 0; }
		[[nodiscard]] bool IsDone() const noexcept { return cursor >= buffer.GetSize(); }

		This& operator<<(char value) noexcept;
		This& operator<<(int8_t value) noexcept;
		This& operator<<(uint8_t value) noexcept;
		This& operator<<(int16_t value) noexcept;
		This& operator<<(uint16_t value) noexcept;
		This& operator<<(int32_t value) noexcept;
		This& operator<<(uint32_t value) noexcept;
		This& operator<<(int64_t value) noexcept;
		This& operator<<(uint64_t value) noexcept;
#ifndef PLATFORM_LINUX
		This& operator<<(size_t value) noexcept;
#endif // PLATFORM_LINUX
		This& operator<<(float value) noexcept;
		This& operator<<(double value) noexcept;
		This& operator<<(long double value) noexcept;
		This& operator<<(const char* str) noexcept;

		template<typename T, size_t N>
		This& operator<<(T (&array)[N]) noexcept
		{
			Put<T>(array, N);
			return *this;
		}

		template<size_t N>
		This& operator<<(const hbe::HInlineString<N>& str) noexcept
		{
			return *this << str.c_str();
		}

		This& operator<<(const hbe::HString& str) noexcept { return *this << str.c_str(); }

		This& operator<<(StaticString str) noexcept { return *this << str.c_str(); }

	private:
		[[nodiscard]] bool IsValidIndex(size_t index) const noexcept { return cursor < buffer.GetSize(); }

		template<typename T>
		void Put(T value) noexcept
		{
			Assert(std::this_thread::get_id() == threadID);
			const size_t size = buffer.GetSize();
			if (cursor >= size) return;

			constexpr size_t tSize = sizeof(T);
			static_assert(tSize > 0);

			const size_t startIndex = ((cursor + tSize - 1) / tSize) * tSize;
			const auto newIndex = startIndex + tSize;

			auto bufferBase = buffer.GetData();
			if (bufferBase == nullptr)
			{
				if (newIndex <= size)
				{
					cursor = newIndex;
				}

				return;
			}

			Assert(startIndex <= newIndex);
			if (newIndex > size)
			{
				++errorCount;
				return;
			}

			while (cursor < startIndex)
			{
				bufferBase[cursor++] = 0;
			}

			auto data = reinterpret_cast<T*>(&bufferBase[cursor]);
			data[0] = value;

			cursor = newIndex;
		}

		template<typename T>
		void Put(const T* value, size_t length) noexcept
		{
			Assert(std::this_thread::get_id() == threadID);

			const size_t size = buffer.GetSize();
			if (cursor >= size) return;

			Put<size_t>(length);

			constexpr size_t tSize = sizeof(T);
			const size_t startIndex = ((cursor + tSize - 1) / tSize) * tSize;
			const auto newIndex = startIndex + (tSize * length);

			auto bufferBase = buffer.GetData();
			if (bufferBase == nullptr)
			{
				if (newIndex <= size)
				{
					cursor = newIndex;
				}

				return;
			}

			Assert(startIndex <= newIndex);
			if (newIndex > size)
			{
				++errorCount;
				return;
			}

			while (cursor < startIndex)
			{
				bufferBase[cursor++] = 0;
			}

			auto data = reinterpret_cast<T*>(&bufferBase[cursor]);
			for (size_t i = 0; i < length; ++i)
			{
				data[i] = value[i];
			}

			cursor = newIndex;
		}

		Buffer& buffer;
		size_t cursor;
		size_t errorCount;
		std::thread::id threadID;
	};

} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{
	class BufferOutputStreamTest final : public TestCollection
	{
	public:
		BufferOutputStreamTest();
		~BufferOutputStreamTest() override = default;

	protected:
		void Prepare() override;
	};
} // namespace hbe
#endif //__UNIT_TEST__
