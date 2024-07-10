// Created by mooming.go@gmail.com, 2022

#pragma once

#include "BufferTypes.h"
#include "String/StaticString.h"
#include "System/Debug.h"


namespace HE
{

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
    Buffer(Buffer&& rhs);
    Buffer(const TGenerateBuffer& genFunc);
    Buffer(const TGenerateBuffer& genFunc, const TReleaseBuffer& releaseFunc);
    ~Buffer();

    StaticString GetClassName() const;
    void SetReleaser(TReleaseBuffer&& releaseFunc);

  public:
    template <typename T>
    T* GetDataAs()
    {
        return reinterpret_cast<T*>(data);
    }

    template <typename T>
    const T* GetDataAs() const
    {
        return reinterpret_cast<T*>(data);
    }

    template <typename T>
    size_t TranslateSizeAs() const
    {
        return size / sizeof(T);
    }

    inline uint8_t* GetData() { return data; }
    inline const uint8_t* GetData() const { return data; }
    inline auto GetSize() const { return size; }
};

} // namespace HE

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace HE
{
class BufferTest : public TestCollection
{
  public:
    BufferTest();
    virtual ~BufferTest() = default;

  protected:
    virtual void Prepare() override;
};
} // namespace HE
#endif //__UNIT_TEST__
