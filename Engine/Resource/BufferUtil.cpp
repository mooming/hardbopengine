// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "BufferUtil.h"

#include "Log/Logger.h"
#include "OSAL/OSFileHandle.h"
#include "OSAL/OSInputOutput.h"
#include "OSAL/OSMapSyncMode.h"
#include "String/StringUtil.h"

namespace hbe
{

	namespace BufferUtil
	{

		Buffer generateDummyBuffer(size_t size) noexcept
		{
			auto generator = [size](TSize& outSize, TBufferData& outData)
			{
				outSize = size;
				outData = nullptr;
			};

			return Buffer(generator);
		}

		Buffer GenerateFileBuffer(StaticString path, OS::FileOpenMode openMode, OS::ProtectionMode protection,
								  size_t size)
		{
			using namespace OS;
			using namespace StringUtil;

			static auto log = Logger::get(toFunctionName(__PRETTY_FUNCTION__));

			FileHandle fh;

			auto generator = [&fh, path, openMode, protection, size](TSize& outSize, TBufferData& outData)
			{
				outSize = 0;
				outData = nullptr;

				if (!Open(fh, path, openMode))
				{
					log.outError([path](auto& ls) { ls << "Failed to open " << path; });

					return;
				}

				auto fileSize = fh.getFileSize();
				if (size != 0)
				{
					fileSize = size;
					if (!Truncate(fh, fileSize))
					{
						log.outWarning([path, size](auto& ls)
						{ ls << "Failed to resize the file " << path << " to the given size " << size; });

						return;
					}
				}

				if (fileSize <= 0)
				{
					log.outWarning([path](auto& ls) { ls << "Nothing to map. File size is zero. path = " << path; });

					return;
				}

				auto ptr = mapMemory(fh, fileSize, protection, 0);
				if (unlikely(ptr == nullptr))
				{
					log.outError([path](auto& ls) { ls << "Failed to map " << path; });

					return;
				}

				outSize = fileSize;
				outData = reinterpret_cast<decltype(outData)>(ptr);
			};

			Buffer buffer(generator);
			if (buffer.getData() == nullptr)
			{
				return buffer;
			}

			auto handleData = fh.data;
			fh.invalidate();

			buffer.setReleaser([handleData](TSize size, TBufferData data) mutable
			{
				if (unlikely(data == nullptr))
				{
					return;
				}

				FileHandle fh;
				fh.data = handleData;

				Assert(size > 0);
				auto ptr = static_cast<void*>(data);

				MapSyncMode syncMode;
				syncMode.setSync();

				mapSync(ptr, size, syncMode);
				unmapMemory((void*) data, size);

				OS::Close(std::move(fh));
			});

			return buffer;
		}

		Buffer getFileBuffer(StaticString path)
		{
			using namespace OS;

			FileOpenMode openMode;
			openMode.setReadWrite();

			ProtectionMode protection;
			protection.setReadable();
			protection.setWritable();

			return GenerateFileBuffer(path, openMode, protection);
		}

		Buffer getReadOnlyFileBuffer(StaticString path)
		{
			using namespace OS;

			FileOpenMode openMode;
			openMode.setReadWrite();

			ProtectionMode protection;
			protection.setReadable();

			return GenerateFileBuffer(path, openMode, protection);
		}

		Buffer getWriteOnlyFileBuffer(StaticString path, size_t size)
		{
			using namespace OS;

			FileOpenMode openMode;
			openMode.setReadWrite();

			if (exist(path))
			{
				openMode.setTruncate();
			}
			else
			{
				openMode.setCreate();
			}

			ProtectionMode protection;
			protection.setWritable();

			return GenerateFileBuffer(path, openMode, protection, size);
		}

	} // namespace BufferUtil

} // namespace hbe
