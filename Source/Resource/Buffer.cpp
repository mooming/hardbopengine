// Created by mooming.go@gmail.com, 2022

#include "Buffer.h"

#include "Config/BuildConfig.h"
#include "Log/Logger.h"
#include "OSAL/Intrinsic.h"
#include "String/StringUtil.h"
#include "System/Debug.h"


namespace HE
{

Buffer::Buffer()
    : size(0)
    , data(nullptr)
{
}

Buffer::Buffer(Buffer&& rhs)
    : size(rhs.size)
    , data(rhs.data)
    , releaser(std::move(rhs.releaser))
{
    rhs.size = 0;
    rhs.data = nullptr;
    rhs.releaser = nullptr;
}

Buffer::Buffer(const TGenerateBuffer& genFunc)
{
    genFunc(size, data);
}

Buffer::Buffer(const TGenerateBuffer& genFunc, const TReleaseBuffer& releaseFunc)
{
    genFunc(size, data);
    releaser = releaseFunc;
}

Buffer::~Buffer()
{
    if (data == nullptr)
        return;

    if (unlikely(releaser == nullptr))
    {
        auto log = Logger::Get(GetClassName());
        log.OutWarning([this, func = __func__](auto& ls)
        {
            ls << '[' << func
                << "] Releaser func is null, in spite of data is " << data;
        });

#ifdef __DEBUG__
        size = 0;
        data = nullptr;
#endif // __DEBUG__

        return;
    }

    releaser(size, data);

#ifdef __DEBUG__
    size = 0;
    data = nullptr;
#endif // __DEBUG__
}

StaticString Buffer::GetClassName() const
{
    using namespace StringUtil;
    static auto className = ToCompactClassName(__PRETTY_FUNCTION__);
    return className;
}


void Buffer::SetReleaser(TReleaseBuffer&& releaseFunc)
{
    releaser = std::move(releaseFunc);
    releaseFunc = nullptr;
}

} // HE

#ifdef __UNIT_TEST__
#include "BufferUtil.h"
#include "Memory/MemoryManager.h"
#include "OSAL/OSFileHandle.h"
#include "OSAL/OSInputOutput.h"


namespace HE
{
BufferTest::BufferTest()
    : TestCollection(StringUtil::ToCompactClassName(__PRETTY_FUNCTION__))
{
}

void BufferTest::Prepare()
{
    AddTest("Default Construction", [this](auto& ls)
    {
        Buffer buffer;

        if (buffer.GetSize() != 0)
        {
            ls << "Invalid buffer size = " << buffer.GetSize() << lferr;
        }

        if (buffer.GetData() != nullptr)
        {
            ls << "Invalid buffer data = " << (void*)buffer.GetData() << lferr;
        }
    });

    AddTest("Generation & Release", [this](auto& ls)
    {
        constexpr size_t TestSize = 10;
        constexpr size_t BufferSize = TestSize * sizeof(int);

        auto& mmgr = MemoryManager::GetInstance();

        auto gen = [&mmgr](auto& size, auto& data)
        {
            size = BufferSize;

            auto ptr = mmgr.NewArray<int>(TestSize, -1);
            data = reinterpret_cast<Buffer::TBufferData>(ptr);
        };

        auto rel = [&](auto size, auto data)
        {
            if (size != BufferSize)
            {
                ls << "Invalid Size " << size << ", it should be "
                    << BufferSize << '.' << lferr;
                return;
            }

            if (data == nullptr)
            {
                ls << "Invalid data " << data << ", it should not be null." << lferr;
                return;
            }

            mmgr.DeleteArray<int>((int*)(data), TestSize);
        };

        {
            Buffer buffer(gen, rel);

            if (buffer.GetSize() != BufferSize)
            {
                ls << "Invalid buffer size = " << buffer.GetSize() << lferr;
            }

            if (buffer.GetData() == nullptr)
            {
                ls << "Invalid buffer data = "
                    << (void*)buffer.GetData() << lferr;
            }
        }
    });

    AddTest("Memory Buffer", [this](auto& ls)
    {
        using namespace BufferUtil;

        constexpr int TestSize = 16;
        constexpr int InitialValue = 3;

        auto buffer = GetMemoryBuffer<int>(TestSize, InitialValue);
        int* ptr = reinterpret_cast<int*>(buffer.GetData());
        if (ptr == nullptr)
        {
            ls << "Failed to create a memory buffer" << lferr;
            return;
        }

        for (int i = 0; i < TestSize; ++i)
        {
            if (ptr[i] != InitialValue)
            {
                ls << "An invalid initial value: " << ptr[i] << ", but "
                    << InitialValue << " is expected." << lferr;
            }
        }
    });

    AddTest("File Buffer", [this](auto& ls)
    {
        using namespace BufferUtil;

        constexpr int TestSize = 26;
        auto text = "abcdefghijklmnopqrstuvwxyz";
        StaticString path("file_buffer_test.dat");

        {
            ls << "Prepare " << path << lf;

            using namespace OS;

            FileHandle fh;
            FileOpenMode openMode;
            openMode.SetWriteOnly();
            openMode.SetTruncate();
            openMode.SetCreate();

            if (!Open(fh, path, openMode))
            {
                ls << "File open faile. path = " << path << lferr;
                return;
            }

            auto wSize = Write(fh, (void*)text, TestSize);
            if (wSize != TestSize)
            {
                ls << "Failed to write a file with the path = " << path
                    << ", " << TestSize << " bytes. " << wSize
                    << " bytes are wrriten." << lferr;

                Close(std::move(fh));
                OS::Delete(path);

                return;
            }

            if (!Close(std::move(fh)))
            {
                ls << "File open faile. path = " << path << lferr;
                return;
            }
        }

        {
            ls << lf << "Map read/write test." << lf;

            auto buffer = GetFileBuffer(path);
            
            char* ptr = reinterpret_cast<char*>(buffer.GetData());
            if (ptr == nullptr)
            {
                ls << "Failed to create a memory buffer" << lferr;
                return;
            }

            for (int i = 0; i < TestSize; ++i)
            {
                ls << "ptr[" << i << "] = " << ptr[i] << " <=> text["
                    << i << "] = " << text[i] << lf;

                if (ptr[i] != text[i])
                {
                    ls << "Invalid character (" << ptr[i] << "), but ("
                        << text[i] << ") is expected." << lferr;
                }

                ptr[i] = 'a';
            }

//            OS::MapSyncMode syncMode;
//            syncMode.SetSync();
//            OS::MapSync(ptr, buffer.GetSize(), syncMode);
        }

        {
            ls << lf << "Read back test." << lf;
            using namespace OS;
            FileHandle fh;
            FileOpenMode openMode;
            openMode.SetReadOnly();

            ls << "Open: " << path << lf;

            if (!Open(fh, path, openMode))
            {
                ls << "File open faile. path = " << path << lferr;
                return;
            }

            for (int i = 0; i < TestSize; ++i)
            {
                uint8_t ch = 0;
                auto rSize = Read(fh, &ch, 1);

                ls << i << " : read (" << ch << ") from the file. <=> 'a'" << lf;
                if (rSize != 1)
                {
                    ls << "Read failed. Read size = " << rSize << lferr;

                    Close(std::move(fh));
                    OS::Delete(path);

                    return;
                }

                if (ch != 'a')
                {
                    ls << "Invalid character is read. ch = (" << ch
                        << "), but 'a' is expected."<< lferr;
                }
            }

            if (!Close(std::move(fh)))
            {
                ls << "File open faile. path = " << path << lferr;
                return;
            }
        }

        if (!OS::Exist(path))
        {
            ls << "File doesn't exist at path = " << path << lferr;
        }
        else
        {
            OS::Delete(path);
        }
    });
}

} // HE
#endif //__UNIT_TEST__

