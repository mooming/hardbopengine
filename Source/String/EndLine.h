// Created by mooming.go@gmail.com, 2022

namespace HE
{

struct EndLine final
{
    operator const char* () const
    {
        return "\n";
    }
};

static constexpr EndLine hendl;

} // HE
