// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstddef>
#include <utility>

#include "Container/Deque.h"
#include "Core/Debug.h"

namespace hbe
{

	template<typename TElement, class TContainer = Deque<TElement>>
	class Queue final
	{
	public:
		using TContainerType = TContainer;

	private:
		TContainer container;

	public:
		Queue() = default;
		Queue(const Queue&) = delete;
		Queue& operator=(const Queue&) = delete;

		Queue(Queue&& rhs) noexcept
			: container(std::move(rhs.container))
		{
		}

		Queue& operator=(Queue&& rhs) noexcept
		{
			container = std::move(rhs.container);
			return *this;
		}

		void Push(const TElement& value) { container.PushBack(value); }
		void Push(TElement&& value) { container.PushBack(std::move(value)); }

		template<typename... Types>
		TElement& Emplace(Types&&... args)
		{
			return container.EmplaceBack(std::forward<Types>(args)...);
		}

		void Pop()
		{
			FatalAssert(!container.IsEmpty());
			container.PopFront();
		}

		TElement& Front() { return container.Front(); }
		const TElement& Front() const { return container.Front(); }
		TElement& Back() { return container.Back(); }
		const TElement& Back() const { return container.Back(); }

		[[nodiscard]] auto Size() const { return container.Size(); }
		[[nodiscard]] bool IsEmpty() const { return container.IsEmpty(); }
		void Clear() { container.Clear(); }
	};

} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{

	class QueueTest : public TestCollection
	{
	public:
		QueueTest() : TestCollection("QueueTest") {}

	protected:
		void Prepare() override;
	};

} // namespace hbe
#endif //__UNIT_TEST__
