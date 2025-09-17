// Created by mooming.go@gmail.com, 2017

#pragma once

#include <cstddef>
#include "Debug.h"

#define ONCE while (false)

namespace hbe
{
	class True_t
	{};
	class False_t
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
	inline Type GetAs(void* src)
	{
		Assert((((size_t) (src)) % sizeof(Type)) == 0, "Misaligned Address(", src,
			   ") is provided. Required alignment is ", sizeof(Type), ".");
		return *reinterpret_cast<Type*>(src);
	}

	template<typename Type>
	inline void SetAs(void* dst, Type value)
	{
		Assert((((size_t) (dst)) % sizeof(Type)) == 0, "Misaligned Address(", dst,
			   ") is provided. Required alignment is ", sizeof(Type), ".");
		Type* _dst = reinterpret_cast<Type*>(dst);
		*_dst = value;
	}

	template<typename Type>
	inline void CopyAs(void* dst, void* src)
	{
		Type* _src = reinterpret_cast<Type*>(src);
		Type* _dst = reinterpret_cast<Type*>(dst);
		*_dst = *_src;
	}

	inline size_t ToAddress(void* ptr) { return reinterpret_cast<size_t>(ptr); }

	template<typename T, size_t size>
	inline size_t CountOf(T (&)[size])
	{
		return size;
	}
} // namespace hbe
