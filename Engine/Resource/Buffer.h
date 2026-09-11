// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "BufferTypes.h"
#include "Core/Debug.h"
#include "String/StaticString.h"

namespace hbe
{

	/// @brief A dynamically allocated memory buffer with lazy initialization and release callback.
	class Buffer final
	{
	public:
		using TSize = BufferTypes::TSize;
		using TBufferData = BufferTypes::TBufferData;
		using TGenerateBuffer = BufferTypes::TGenerateBuffer;
		using TReleaseBuffer = BufferTypes::TReleaseBuffer;

		Buffer();
		Buffer(Buffer&& rhs) noexcept;
		explicit Buffer(const TGenerateBuffer& genFunc);
		Buffer(const TGenerateBuffer& genFunc, const TReleaseBuffer& releaseFunc);
		~Buffer();

		[[nodiscard]] StaticString getClassName() const noexcept;
		void setReleaser(TReleaseBuffer&& releaseFunc);

		template<typename T>
		T* getDataAs()
		{
			return reinterpret_cast<T*>(data);
		}

		template<typename T>
		const T* getDataAs() const
		{
			return reinterpret_cast<T*>(data);
		}

		template<typename T>
		[[nodiscard]] size_t translateSizeAs() const noexcept
		{
			return size / sizeof(T);
		}

		[[nodiscard]] uint8_t* getData() noexcept { return data; }
		[[nodiscard]] const uint8_t* getData() const noexcept { return data; }
		[[nodiscard]] auto getSize() const noexcept { return size; }

	private:
		TSize size;
		TBufferData data;
		TReleaseBuffer releaser;
	};

} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{
	class BufferTest final : public TestCollection
	{
	public:
		BufferTest();
		~BufferTest() override = default;

	protected:
		void prepare() override;
	};
} // namespace hbe
#endif //__UNIT_TEST__
