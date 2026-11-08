// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

#include "Core/Debug.h"


namespace hbe
{
template <typename TSignature, size_t InlineCapacity = 64>
class HInlineFunction;

template <typename TReturn, typename... TArgs, size_t InlineCapacity>
class HInlineFunction<TReturn(TArgs...), InlineCapacity> final
{
public:
	using TThis = HInlineFunction;

	static constexpr size_t Capacity = InlineCapacity;
	static constexpr size_t Alignment = alignof(std::max_align_t);

	static_assert(InlineCapacity > 0, "InlineCapacity should be greater than 0.");

private:
	alignas(Alignment) std::byte buffer[InlineCapacity];

	TReturn (*invoker)(void*, TArgs...);
	void (*relocator)(void* from, void* to);
	void (*destroyer)(void*);

public:
	HInlineFunction() noexcept
		: invoker(nullptr)
		, relocator(nullptr)
		, destroyer(nullptr)
	{
	}

	HInlineFunction(const HInlineFunction&) = delete;
	HInlineFunction& operator=(const HInlineFunction&) = delete;

	template <typename TCallable>
	HInlineFunction(TCallable&& callable) noexcept
	{
		using TCallableType = std::decay_t<TCallable>;

		static_assert(!std::is_same_v<TCallableType, TThis>,
					  "HInlineFunction cannot wrap another HInlineFunction. Move it instead.");
		static_assert(sizeof(TCallableType) <= InlineCapacity,
					  "HInlineFunction inline capacity exceeded. Trim the captures or grow InlineCapacity.");
		static_assert(alignof(TCallableType) <= Alignment,
					  "HInlineFunction alignment exceeds the inline storage alignment.");

		new (buffer) TCallableType(std::forward<TCallable>(callable));
		invoker = &Invoke<TCallableType>;
		relocator = &Relocate<TCallableType>;
		destroyer = &Destroy<TCallableType>;
	}

	HInlineFunction(HInlineFunction&& rhs) noexcept
		: invoker(rhs.invoker)
		, relocator(rhs.relocator)
		, destroyer(rhs.destroyer)
	{
		rhs.invoker = nullptr;
		rhs.relocator = nullptr;
		rhs.destroyer = nullptr;

		if (invoker != nullptr)
		{
			relocator(rhs.buffer, buffer);
		}
	}

	~HInlineFunction()
	{
		if (invoker != nullptr)
		{
			destroyer(buffer);
		}
	}

	TThis& operator=(HInlineFunction&& rhs) noexcept
	{
		if (this == &rhs)
		{
			return *this;
		}

		if (invoker != nullptr)
		{
			destroyer(buffer);
		}

		invoker = rhs.invoker;
		relocator = rhs.relocator;
		destroyer = rhs.destroyer;
		rhs.invoker = nullptr;
		rhs.relocator = nullptr;
		rhs.destroyer = nullptr;

		if (invoker != nullptr)
		{
			relocator(rhs.buffer, buffer);
		}

		return *this;
	}

	[[nodiscard]] bool IsValid() const noexcept
	{
		return invoker != nullptr;
	}

	TReturn operator()(TArgs... args) const
	{
		Assert(IsValid());

		return invoker(const_cast<std::byte*>(buffer), std::forward<TArgs>(args)...);
	}

private:
	template <typename TCallableType>
	static TReturn Invoke(void* storage, TArgs... args)
	{
		return (*reinterpret_cast<TCallableType*>(storage))(std::forward<TArgs>(args)...);
	}

	template <typename TCallableType>
	static void Relocate(void* from, void* to)
	{
		auto& callable = *reinterpret_cast<TCallableType*>(from);
		new (to) TCallableType(std::move(callable));
		callable.~TCallableType();
	}

	template <typename TCallableType>
	static void Destroy(void* storage)
	{
		reinterpret_cast<TCallableType*>(storage)->~TCallableType();
	}
};
} // namespace hbe

#ifdef TEST_ENABLED
#include "Test/TestCollection.h"

namespace hbe
{
class HInlineFunctionTest final : public TestCollection
{
public:
	HInlineFunctionTest()
		: TestCollection("HInlineFunctionTest")
	{
	}

protected:
	void Prepare() override;
};
} // namespace hbe
#endif // TEST_ENABLED
