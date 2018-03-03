// Copyright Hansol Park (anav96@naver.com, mooming.go@gmail.com). All rights reserved.

#ifndef StdUtil
#define StdUtil

#define ONCE while(false)

#include <cstddef>

namespace HE
{
    class True_t {};
    class False_t {};

    template <typename T>
    struct IsReferenceType { using Result = False_t; };

    template <typename T>
    struct IsReferenceType<T&> { using Result = True_t; };

    template <typename Type>
    inline Type GetAs(void *src)
    {
        return *reinterpret_cast<Type*>(src);
    }

    template <typename Type>
    inline void SetAs(void *dst, Type value)
    {
        Type* _dst = reinterpret_cast<Type*>(dst);
        *_dst = value;
    }

    template <typename Type>
    inline void CopyAs(void *dst, void *src)
    {
        Type* _src = reinterpret_cast<Type*>(src);
        Type* _dst = reinterpret_cast<Type*>(dst);
        *_dst = *_src;
    }

    inline size_t ToAddress(void* ptr)
    {
        return reinterpret_cast<size_t>(ptr);
    }

    extern bool Equals(float a, float b);
    extern bool Equals(double a, double b);
}

#endif /* StdUtil */
