// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "Debug.h"

#define ONCE while (false)

namespace hbe
{
class True_t final
{};
class False_t final
{};

template<typename T>
struct IsReferenceType
{
	using Result = False_t;
	static constexpr size_t TypeSize = sizeof(T);
};

template<typename T>
struct IsReferenceType<T&>
{
	using Result = True_t;
	static constexpr size_t TypeSize = sizeof(T*);
};

template<typename Type>
Type GetAs(void* src) noexcept
{
	Assert((((size_t) (src)) % sizeof(Type)) == 0, "Misaligned Address(", src,
		   ") is provided. Required alignment is ", sizeof(Type), ".");
	return *reinterpret_cast<Type*>(src);
}

template<typename Type>
void SetAs(void* dst, Type value) noexcept
{
	Assert((((size_t) (dst)) % sizeof(Type)) == 0, "Misaligned Address(", dst,
		   ") is provided. Required alignment is ", sizeof(Type), ".");
	Type* _dst = reinterpret_cast<Type*>(dst);
	*_dst = value;
}

template<typename Type>
void CopyAs(void* dst, void* src) noexcept
{
	Type* _src = reinterpret_cast<Type*>(src);
	Type* _dst = reinterpret_cast<Type*>(dst);
	*_dst = *_src;
}

[[nodiscard]] inline size_t ToAddress(void* ptr) noexcept { return reinterpret_cast<size_t>(ptr); }

template<typename T, size_t size>
[[nodiscard]] size_t CountOf(T (&)[size]) noexcept
{
	return size;
}
} // namespace hbe
