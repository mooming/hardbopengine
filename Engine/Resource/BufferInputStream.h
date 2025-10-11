// Created by mooming.go@gmail.com, 2022

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

	class BufferInputStream final
	{
	public:
		using This = BufferInputStream;
		template<typename T>
		using TVector = hbe::HVector<T>;

	private:
		const Buffer& buffer;
		size_t cursor;
		size_t errorCount;

	public:
		BufferInputStream(const Buffer& buffer);
		~BufferInputStream() = default;

		inline auto GetErrorCount() const { return errorCount; }
		inline bool HasError() const { return errorCount > 0; }
		inline void ClearErrorCount() { errorCount = 0; }
		inline bool IsDone() const { return cursor >= buffer.GetSize(); }

	public:
		This& operator>>(char& value);
		This& operator>>(int8_t& value);
		This& operator>>(uint8_t& value);
		This& operator>>(int16_t& value);
		This& operator>>(uint16_t& value);
		This& operator>>(int32_t& value);
		This& operator>>(uint32_t& value);
		This& operator>>(int64_t& value);
		This& operator>>(uint64_t& value);
#ifndef PLATFORM_LINUX
		This& operator>>(size_t& value);
#endif // PLATFORM_LINUX
		This& operator>>(float& value);
		This& operator>>(double& value);
		This& operator>>(long double& value);

		This& operator>>(StaticString& str);
		This& operator>>(const hbe::HString& str);

		template<typename T, size_t N>
		This& operator>>(T (&array)[N])
		{
			Get<T>(array, N);
			return *this;
		}

		template<typename T, class TContainer = TVector<T>>
		This& operator>>(TContainer& container)
		{
			Get<T>(container);
			return *this;
		}

	private:
		inline bool IsValidIndex(size_t index) const { return cursor < buffer.GetSize(); }

		template<typename T>
		void Get(T& value, const T& defaultValue)
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
		void Get(T* arrayBuffer, size_t size)
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
		void Get(TContainer& array)
		{
			static_assert(std::is_same<T, typename TContainer::value_type>::value);

			array.clear();

			size_t length = 0;
			Get<size_t>(length, 0);

			if (length <= 0)
			{
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

			array.reserve(length);

			auto bufferBase = buffer.GetData();
			auto dataBegin = reinterpret_cast<const T*>(&bufferBase[cursor]);
			auto dataEnd = dataBegin + length;
			std::copy(dataBegin, dataEnd, std::back_inserter(array));

			cursor = newIndex;
		}
	};

} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{
	class BufferInputStreamTest : public TestCollection
	{
	public:
		BufferInputStreamTest();
		virtual ~BufferInputStreamTest() = default;

	protected:
		virtual void Prepare() override;
	};
} // namespace hbe
#endif //__UNIT_TEST__
