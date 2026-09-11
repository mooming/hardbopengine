// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <limits>

#include "Buffer.h"
#include "BufferTypes.h"
#include "Core/Debug.h"
#include "Memory/MemoryManager.h"
#include "OSAL/OSFileOpenMode.h"
#include "OSAL/OSProtectionMode.h"
#include "String/StaticString.h"

namespace OS
{
	class FileHandle;
};

namespace hbe
{
	/// @brief Utility functions for creating and managing buffers from various sources.
	namespace BufferUtil
	{
		using namespace BufferTypes;

		[[nodiscard]] Buffer generateDummyBuffer(size_t size = std::numeric_limits<size_t>::max()) noexcept;
		[[nodiscard]] Buffer GenerateFileBuffer(StaticString path, OS::FileOpenMode openMode, OS::ProtectionMode protection,
								  size_t size = 0);
		[[nodiscard]] Buffer getFileBuffer(StaticString path);
		[[nodiscard]] Buffer getReadOnlyFileBuffer(StaticString path);
		[[nodiscard]] Buffer getWriteOnlyFileBuffer(StaticString path, size_t size);

		template<typename T>
		[[nodiscard]] Buffer getMemoryBuffer(TSize numElements, const T& defaultValue)
		{
			auto& mmgr = MemoryManager::getInstance();
			auto generator = [&mmgr, numElements, defaultValue](TSize& outSize, TBufferData& outData)
			{
				outSize = numElements * sizeof(T);
				auto ptr = mmgr.newArray<T>(numElements, defaultValue);
				outData = reinterpret_cast<TBufferData>(ptr);
			};

			auto releaser = [&mmgr](TSize size, TBufferData data)
			{
				const TSize numElements = size / sizeof(T);
				Assert((numElements * sizeof(T)) == size);

				auto ptr = reinterpret_cast<T*>(data);
				mmgr.deleteArray<T>(ptr, numElements);
			};

			return Buffer(generator, releaser);
		}

	} // namespace BufferUtil
} // namespace hbe
