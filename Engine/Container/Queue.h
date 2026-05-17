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

		void Push(const TElement& value) noexcept { container.PushBack(value); }
		void Push(TElement&& value) noexcept { container.PushBack(std::move(value)); }

		template<typename... Types>
		TElement& Emplace(Types&&... args) noexcept
		{
			return container.EmplaceBack(std::forward<Types>(args)...);
		}

		void Pop() noexcept
		{
			FatalAssert(!container.IsEmpty());
			container.PopFront();
		}

		TElement& Front() noexcept { return container.Front(); }
		const TElement& Front() const noexcept { return container.Front(); }
		TElement& Back() noexcept { return container.Back(); }
		const TElement& Back() const noexcept { return container.Back(); }

		[[nodiscard]] auto Size() const noexcept { return container.Size(); }
		[[nodiscard]] bool IsEmpty() const noexcept { return container.IsEmpty(); }
		void Clear() noexcept { container.Clear(); }

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
		void Prepare() override;
	};

} // namespace hbe
#endif //__UNIT_TEST__
