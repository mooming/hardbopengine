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

		Queue() = default;

		Queue(const Queue&) = delete;

		Queue(Queue&& rhs) noexcept
			: container(std::move(rhs.container))
		{
		}

		Queue& operator=(const Queue&) = delete;

		Queue& operator=(Queue&& rhs) noexcept
		{
			container = std::move(rhs.container);
			return *this;
		}

		void push(const TElement& value) noexcept { container.pushBack(value); }
		void push(TElement&& value) noexcept { container.pushBack(std::move(value)); }

		template<typename... Types>
		TElement& emplace(Types&&... args) noexcept
		{
			return container.emplaceBack(std::forward<Types>(args)...);
		}

		void pop() noexcept
		{
			fatalAssert(!container.IsEmpty());
			container.popFront();
		}

		TElement& front() noexcept { return container.front(); }
		const TElement& front() const noexcept { return container.front(); }
		TElement& back() noexcept { return container.back(); }
		const TElement& back() const noexcept { return container.back(); }

		[[nodiscard]] auto Size() const noexcept { return container.Size(); }
		[[nodiscard]] bool IsEmpty() const noexcept { return container.IsEmpty(); }
		void clear() noexcept { container.clear(); }

	private:
		TContainer container;
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
		void prepare() override;
	};

} // namespace hbe
#endif //__UNIT_TEST__
