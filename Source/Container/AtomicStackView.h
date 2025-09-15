// Created by mooming.go@gmail.com, 2022

#pragma once

#include <atomic>
#include "OSAL/Intrinsic.h"

namespace hbe
{

	template<typename T>
	concept CNext = requires(T t) { t.next; };

	template<CNext T>
	class AtomicStackView
	{
	private:
		static_assert(std::atomic<T*>::is_always_lock_free,
					  "The speicified type is not always lock free on this platfrom.");
		std::atomic<T*> top;

	public:
		using Iterator = T*;
		using ConstIterator = const T*;

	public:
		AtomicStackView() : top(nullptr) {}

		~AtomicStackView() = default;

		void Push(T& newItem)
		{
			newItem.next = top.load(std::memory_order_relaxed);

			while (!top.compare_exchange_weak(newItem.next, &newItem, std::memory_order_release,
											  std::memory_order_relaxed))
				;
		}

		T* Pop()
		{
			T* node = top.load(std::memory_order_relaxed);
			if (unlikely(node == nullptr))
			{
				return nullptr;
			}

			while (!top.compare_exchange_weak(node, node->next, std::memory_order_release, std::memory_order_relaxed))
			{
				if (node == nullptr)
				{
					return nullptr;
				}
			}

			node->next = nullptr;

			return node;
		}

		[[nodiscard]] bool IsEmpty() const { return top.load(std::memory_order_relaxed) == nullptr; }
	};

} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{

	class AtomicStackViewTest : public TestCollection
	{
	public:
		AtomicStackViewTest() : TestCollection("AtomicStackViewTest") {}

	protected:
		void Prepare() override;
	};

} // namespace hbe

#endif //__UNIT_TEST__
