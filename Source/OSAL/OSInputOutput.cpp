// Created by mooming.go@gmail.com 2022

#include "OSInputOutput.h"

#ifdef __UNIT_TEST__
#include "OSFileHandle.h"
#include "OSFileOpenMode.h"
#include "OSMapSyncMode.h"
#include "OSProtectionMode.h"
#include "String/StringUtil.h"
#include "Test/TestCollection.h"

namespace hbe
{

	OSInputOutputTest::OSInputOutputTest() : TestCollection(StringUtil::ToCompactClassName(__PRETTY_FUNCTION__)) {}

	void OSInputOutputTest::Prepare()
	{
		using namespace OS;
		static const StaticString path("Test.data");

		AddTest("Open/Close/Delete", [&, this](auto& ls)
		{
			FileHandle fh;
			FileOpenMode openMode;

			openMode.SetReadWrite();
			openMode.SetTruncate();

			if (!Exist(path))
			{
				openMode.SetCreate();
			}

			ls << "Try to open " << path << lf;
			if (Open(fh, path, openMode))
			{
				ls << "File open succeeds. path = " << path << lf;
			}
			else
			{
				ls << "File open failed. path = " << path << lferr;
				return;
			}

			if (Exist(path))
			{
				ls << "The file found! path = " << path << lf;
			}
			else
			{
				ls << "The file NOT found! path = " << path << lferr;
			}

			if (Close(std::move(fh)))
			{
				ls << "File close succeeded. path = " << path << lf;
			}
			else
			{
				ls << "File close failed. path = " << path << lferr;
			}

			if (Delete(path))
			{
				ls << "File removal succeeded. path = " << path << lf;
			}
			else
			{
				ls << "File removal failed. path = " << path << lf;
			}
		});

		AddTest("MapMemory", [&, this](auto& ls)
		{
			constexpr int TestSize = 256;

			{
				FileHandle fh;
				FileOpenMode openMode;

				openMode.SetReadWrite();
				openMode.SetTruncate();

				if (!Exist(path))
				{
					openMode.SetCreate();
				}

				if (!Open(fh, path, openMode))
				{
					ls << "File open failed. path = " << path << lferr;
					return;
				}

				{
					uint8_t buffer[TestSize];
					Write(fh, buffer, TestSize);
				}

				ProtectionMode protection;
				protection.SetReadable();
				protection.SetWritable();

				auto ptr = MapMemory(fh, TestSize, protection, 0);
				if (ptr == nullptr)
				{
					Close(std::move(fh));
					Delete(path);

					ls << "Memory Map failed." << path << lferr;
					return;
				}

				auto buffer = reinterpret_cast<uint8_t*>(ptr);
				for (int i = 0; i < TestSize; ++i)
				{
					buffer[i] = static_cast<uint8_t>(i);
				}

				MapSyncMode syncMode;
				syncMode.SetSync();
				MapSync(ptr, TestSize, syncMode);

				for (int i = 0; i < TestSize; ++i)
				{
					ls << "buffer[" << i << "] = " << i << lf;

					if (buffer[i] != i)
					{
						ls << "Mapped memory lost data. buffer[" << i << "] is supposed to be " << i << ", but "
						   << buffer[i];
					}
				}

				UnmapMemory(ptr, TestSize);
				Close(std::move(fh));
			}

			{
				FileHandle fh;
				FileOpenMode openMode;
				openMode.SetReadOnly();

				if (!Open(fh, path, openMode))
				{
					Delete(path);
					ls << "File open failed. path = " << path << lferr;
					return;
				}

				auto fileSize = fh.GetFileSize();
				if (fileSize != TestSize)
				{
					ls << "File size(" << fileSize << ") is not valid. " << TestSize << " is expected." << lferr;
					return;
				}

				uint8_t buffer[TestSize];
				auto readBytes = Read(fh, buffer, TestSize);
				if (readBytes != TestSize)
				{
					Close(std::move(fh));
					Delete(path);

					ls << "File read failed. Returned Size = " << readBytes << ", but " << TestSize << " is expected."
					   << lferr;

					return;
				}

				ls << "Read " << readBytes << " bytes is success." << lf;

				for (int i = 0; i < TestSize; ++i)
				{
					ls << "Read: buffer[" << i << "] = " << buffer[i] << " <=> " << i << lf;

					if (buffer[i] != i)
					{
						ls << "Mapped memory lost data. Read: buffer[" << i << "] is supposed to be " << i << ", but "
						   << buffer[i];
					}
				}

				Close(std::move(fh));
				Delete(path);
			}
		});
	}

} // namespace hbe
#endif //__UNIT_TEST__
