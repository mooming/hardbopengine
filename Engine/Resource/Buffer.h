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

	private:
		TSize size;
		TBufferData data;
		TReleaseBuffer releaser;

	public:
		Buffer();
		Buffer(Buffer&& rhs) noexcept;
		explicit Buffer(const TGenerateBuffer& genFunc);
		Buffer(const TGenerateBuffer& genFunc, const TReleaseBuffer& releaseFunc);
		~Buffer();

		StaticString GetClassName() const;
		void SetReleaser(TReleaseBuffer&& releaseFunc);

	public:
		template<typename T>
		T* GetDataAs()
		{
			return reinterpret_cast<T*>(data);
		}

		template<typename T>
		const T* GetDataAs() const
		{
			return reinterpret_cast<T*>(data);
		}

		template<typename T>
		size_t TranslateSizeAs() const
		{
			return size / sizeof(T);
		}

		uint8_t* GetData() { return data; }
		const uint8_t* GetData() const { return data; }
		auto GetSize() const { return size; }
	};

} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{
	class BufferTest : public TestCollection
	{
	public:
		BufferTest();
		virtual ~BufferTest() = default;

	protected:
		virtual void Prepare() override;
	};
} // namespace hbe
#endif //__UNIT_TEST__
