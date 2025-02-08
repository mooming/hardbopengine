// Created by mooming.go@gmail.com, 2017

#pragma once

#include "Config/BuildConfig.h"
#include "StaticStringID.h"
#include <ostream>
#include <string_view>

namespace HE
{

    template <typename T>
    concept CToZeroTerminateStr = requires(T t) { t.c_str(); };

    class StaticString final
    {
    private:
        StaticStringID id;

    public:
        StaticString();
        StaticString(StaticStringID id);
        StaticString(const char *string);
        StaticString(const std::string_view &str);

        template <CToZeroTerminateStr T>
        StaticString(const T &string)
            : StaticString(string.c_str())
        {
        }

        ~StaticString() = default;

        const char *c_str() const;

        inline auto GetID() const noexcept { return id; }
        inline bool IsNull() const noexcept { return id.ptr == nullptr; }
        inline operator const char *() const { return c_str(); }
        inline bool operator<(const StaticString &rhs) const
        {
            return id.ptr < rhs.id.ptr;
        }
        inline bool operator==(const StaticString &rhs) const
        {
            return id.ptr == rhs.id.ptr;
        }

        inline friend std::ostream &operator<<(
            std::ostream &os, const StaticString &str)
        {
            os << str.c_str();
            return os;
        }
    };

} // namespace HE

namespace std
{
    template <>
    struct hash<HE::StaticString> final
    {
        std::size_t operator()(const HE::StaticString &obj) const
        {
            return reinterpret_cast<std::size_t>(obj.GetID().ptr);
        }
    };
} // namespace std

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace HE
{

    class StaticStringTest : public TestCollection
    {
    public:
        StaticStringTest()
            : TestCollection("StaticStringTest")
        {
        }

    protected:
        virtual void Prepare() override;
    };

} // namespace HE
#endif //__UNIT_TEST__
