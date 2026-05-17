// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstddef>
#include <cstdint>

#include "Buffer.h"
#include "Core/Debug.h"
#include "HSTL/HString.h"
#include "HSTL/HVector.h"
#include "OSAL/Intrinsic.h"

namespace hbe
{

	/// @brief Stream for reading primitive types and strings from a buffer.
	class BufferInputStream final
	{
	public:
		using This = BufferInputStream;
		template<typename T>
		using TVector = hbe::HVector<T>;

		explicit BufferInputStream(const Buffer& buffer) noexcept;
		~BufferInputStream() = default;

		[[nodiscard]] auto GetErrorCount() const noexcept { return errorCount; }
		[[nodiscard]] bool HasError() const noexcept { return errorCount > 0; }
		void ClearErrorCount() noexcept { errorCount = 0; }
		[[nodiscard]] bool IsDone() const noexcept { return cursor >= buffer.GetSize(); }

		This& operator>>(char& value) noexcept;
		This& operator>>(int8_t& value) noexcept;
		This& operator>>(uint8_t& value) noexcept;
		This& operator>>(int16_t& value) noexcept;
		This& operator>>(uint16_t& value) noexcept;
		This& operator>>(int32_t& value) noexcept;
		This& operator>>(uint32_t& value) noexcept;
		This& operator>>(int64_t& value) noexcept;
		This& operator>>(uint64_t& value) noexcept;
#ifndef PLATFORM_LINUX
		This& operator>>(size_t& value) noexcept;
#endif // PLATFORM_LINUX
		This& operator>>(float& value) noexcept;
		This& operator>>(double& value) noexcept;
		This& operator>>(long double& value) noexcept;

		This& operator>>(StaticString& str) noexcept;
		This& operator>>(const hbe::HString& str) noexcept;

		template<typename T, size_t N>
		This& operator>>(T (&array)[N]) noexcept
		{
			Get<T>(array, N);
			return *this;
		}

		template<typename T, class TContainer = TVector<T>>
		This& operator>>(TContainer& container) noexcept
		{
			Get<T>(container);
			return *this;
		}

	private:
		[[nodiscard]] bool IsValidIndex(size_t index) const noexcept { return cursor < buffer.GetSize(); }

		template<typename T>
		void Get(T& value, const T& defaultValue) noexcept
		{
			constexpr size_t tSize = sizeof(T);
			const size_t startIndex = ((cursor + tSize - 1) / tSize) * tSize;
			const size_t newIndex = startIndex + tSize;

			if (unlikely(!IsValidIndex(newIndex)))
			{
				++errorCount;
				value = defaultValue;
				return;
			}

			cursor = startIndex;

			auto bufferBase = buffer.GetData();
			auto data = reinterpret_cast<const T*>(&bufferBase[cursor]);
			value = *data;

			cursor = newIndex;
		}

		template<typename T>
		void Get(T* arrayBuffer, size_t size) noexcept
		{
			size_t length = 0;
			Get<size_t>(length, 0);

			if (unlikely(length != size))
			{
				++errorCount;
				return;
			}

			constexpr size_t tSize = sizeof(T);
			const size_t startIndex = ((cursor + tSize - 1) / tSize) * tSize;
			const size_t newIndex = startIndex + (tSize * length);

			if (unlikely(!IsValidIndex(newIndex)))
			{
				++errorCount;
				return;
			}

			auto bufferBase = buffer.GetData();
			auto dataBegin = reinterpret_cast<const T*>(&bufferBase[cursor]);
			auto dataEnd = dataBegin + length;
			std::copy(dataBegin, dataEnd, arrayBuffer);

			cursor = newIndex;
		}

		template<typename T, class TContainer = TVector<T>>
		void Get(TContainer& array) noexcept
		{
			static_assert(std::is_same<T, typename TContainer::value_type>::value);

			array.clear();

			size_t length = 0;
			Get<size_t>(length, 0);

			if (length <= 0) return;

			constexpr size_t tSize = sizeof(T);
			const size_t startIndex = ((cursor + tSize - 1) / tSize) * tSize;
			const size_t newIndex = startIndex + (tSize * length);

			if (unlikely(!IsValidIndex(newIndex)))
			{
				++errorCount;
				return;
			}

			array.reserve(length);

			auto bufferBase = buffer.GetData();
			auto dataBegin = reinterpret_cast<const T*>(&bufferBase[cursor]);
			auto dataEnd = dataBegin + length;
			std::copy(dataBegin, dataEnd, std::back_inserter(array));

			cursor = newIndex;
		}

		const Buffer& buffer;
		size_t cursor;
		size_t errorCount;
	};

} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{
	class BufferInputStreamTest final : public TestCollection
	{
	public:
		BufferInputStreamTest();
		~BufferInputStreamTest() override = default;

	protected:
		void Prepare() override;
	};
} // namespace hbe
#endif //__UNIT_TEST__
