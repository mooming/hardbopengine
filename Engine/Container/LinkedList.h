// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <functional>
#include "Core/Debug.h"
#include "Memory/DefaultAllocator.h"
#include "Memory/Memory.h"

namespace hbe
{
	/// @brief Node structure for doubly-linked list containing value and pointers to adjacent nodes
	template<typename TType>
	struct LinkedListNode final
	{
		using This = LinkedListNode;
		TType value;
		LinkedListNode* previous;
		LinkedListNode* next;

		explicit LinkedListNode(const TType& value) noexcept : value(value), previous(nullptr), next(nullptr) {}

		explicit LinkedListNode(TType&& value) noexcept : value(std::move(value)), previous(nullptr), next(nullptr) {}

		bool operator!=(const This& rhs) const noexcept { return this != &rhs; }
		TType& operator*() noexcept { return value; }
		const TType& operator*() const noexcept { return value; }
		bool IsHead() const noexcept { return previous == nullptr; }
		bool IsTail() const noexcept { return next == nullptr; }
	};

	/// @brief Doubly-linked list implementation with custom allocator support
	template<typename TType, class TAllocator = DefaultAllocator<LinkedListNode<TType>>>
	class LinkedList final
	{
	public:
		using Node = LinkedListNode<TType>;

	public:
		class Iterator
		{
		private:
			Node* node;

		public:
			Iterator(Node* node) noexcept : node(node) {}
			void operator++() noexcept { node = node->next; }
			bool operator!=(const Iterator& rhs) const noexcept { return node != rhs.node; }
			TType& operator*() noexcept { return node->value; }
			const TType& operator*() const noexcept { return node->value; }
		};

		using ConstIterator = Iterator;

	private:
		Node* head;
		Node* tail;
		TAllocator allocator;

	public:
		LinkedList(const LinkedList&) = delete;
		LinkedList& operator=(const LinkedList&) = delete;

	public:
		LinkedList() noexcept : head(nullptr), tail(nullptr) {}

		LinkedList(LinkedList&& rhs) noexcept : head(rhs.head), tail(rhs.tail)
		{
			rhs.head = nullptr;
			rhs.tail = nullptr;
		}

		LinkedList& operator=(LinkedList&& rhs) noexcept
		{
			Node* tmpHead = rhs.head;
			Node* tmpTail = rhs.tail;

			rhs.head = nullptr;
			rhs.tail = nullptr;

			head = tmpHead;
			tail = tmpTail;

			return *this;
		}

		~LinkedList() noexcept { Clear(); }

	public:
		Iterator begin() noexcept { return Iterator(head); }
		Iterator end() noexcept { return Iterator(nullptr); }
		ConstIterator begin() const noexcept { return ConstIterator(head); }
		ConstIterator end() const noexcept { return ConstIterator(nullptr); }

	public:
		[[nodiscard]] bool IsEmpty() const noexcept
		{
			Assert(head != nullptr || head == tail);
			return head == nullptr;
		}

		void Clear() noexcept
		{
			while (head != nullptr)
			{
				RemoveNode(head);
			}
		}

		Iterator Remove(const TType& element) noexcept
		{
			Assert(ContainsElement(element));
			return Iterator(RemoveNode(GetNodeOf(element)));
		}

		TType& Add(const TType& value) noexcept { return AddLast(value); }

		TType& Add(TType&& value) noexcept { return AddLast(std::move(value)); }

		[[nodiscard]] bool Contains(const TType& value) const noexcept
		{
			for (auto& element : *this)
			{
				if (element == value)
					return true;
			}

			return false;
		}

		[[nodiscard]] bool Contains(const TType* ptr) const noexcept
		{
			for (auto& element : *this)
			{
				if (&element == ptr)
					return true;
			}

			return false;
		}

		[[nodiscard]] TType* Find(const TType& value) noexcept
		{
			for (auto& element : *this)
			{
				if (element == value)
					return &element;
			}

			return nullptr;
		}

		[[nodiscard]] const TType* Find(const TType& value) const noexcept
		{
			for (auto& element : *this)
			{
				if (element == value)
					return &element;
			}

			return nullptr;
		}

		[[nodiscard]] Index Count(const TType& value) const noexcept
		{
			int count = 0;
			for (auto& element : *this)
			{
				if (element == value)
				{
					++count;
				}
			}
			return count;
		}

		bool FindAndRemove(const TType& value) noexcept
		{
			if (auto found = Find(value))
			{
				Remove(*found);
				return true;
			}

			return false;
		}

	public:
		TType& AddFirst(const TType& value) noexcept { return AddPrevious(head, New<Node>(allocator, value))->value; }

		TType& AddFirst(TType&& value) noexcept
		{
			return AddPrevious(head, New<Node>(allocator, std::forward<TType&&>(value)))->value;
		}

		TType& AddLast(const TType& value) noexcept { return AddNext(tail, New<Node>(allocator, value))->value; }

		TType& AddLast(TType&& value) noexcept
		{
			return AddNext(tail, New<Node>(allocator, std::forward<TType&&>(value)))->value;
		}

		TType& AddPrevious(TType& current, const TType& value) noexcept
		{
			return AddPrevious(GetNodeOf(current), New<Node>(allocator, value))->value;
		}

		TType& AddPrevious(TType& current, TType&& value) noexcept
		{
			return AddPrevious(GetNodeOf(current), New<Node>(allocator, std::forward<TType&&>(value)))->value;
		}

		TType& AddNext(TType& current, const TType& value) noexcept
		{
			return AddNext(GetNodeOf(current), New<Node>(allocator, value))->value;
		}

		TType& AddNext(TType& current, TType&& value) noexcept
		{
			return AddNext(GetNodeOf(current), New<Node>(allocator, std::forward(value)))->value;
		}

	private:
		Node* RemoveNode(Node* node) noexcept
		{
			auto next = node->next;

			if (node == head)
			{
				head = next;
			}
			else if (node == tail)
			{
				tail = node->previous;
			}

			Unlink(node);

			return next;
		}

		Node* GetNodeOf(TType& element) noexcept
		{
			Assert(ContainsElement(element));
			return reinterpret_cast<Node*>(&element);
		}

		Node* AddPrevious(Node* current, Node* node) noexcept
		{
			Assert((current != nullptr || IsEmpty()) && node != nullptr);

			if (IsEmpty())
			{
				head = node;
				tail = node;
			}
			else
			{
				LinkPrevious(current, node);
				if (current == head)
				{
					head = node;
				}
			}

			return node;
		}

		Node* AddNext(Node* current, Node* node) noexcept
		{
			Assert((current != nullptr || IsEmpty()) && node != nullptr);

			if (IsEmpty())
			{
				head = node;
				tail = node;
			}
			else
			{
				LinkNext(current, node);
				if (current == tail)
				{
					tail = node;
				}
			}

			return node;
		}

	private:
		void LinkPrevious(Node* node, Node* newNode) noexcept
		{
			Assert(node != nullptr);
			Assert(newNode != nullptr);

			auto prev = node->previous;

			node->previous = newNode;
			newNode->next = node;
			newNode->previous = prev;

			if (prev)
			{
				prev->next = newNode;
			}
		}

		void LinkNext(Node* node, Node* newNode) noexcept
		{
			Assert(node != nullptr);
			Assert(newNode != nullptr);

			auto next = node->next;

			node->next = newNode;
			newNode->previous = node;
			newNode->next = next;

			if (next)
			{
				next->previous = newNode;
			}
		}

		void Unlink(Node* node) noexcept
		{
			Assert(node != nullptr);

			auto prev = node->previous;
			auto next = node->next;

			Delete<Node>(allocator, node);

			if (prev != nullptr)
			{
				prev->next = next;
			}

			if (next != nullptr)
			{
				next->previous = prev;
			}
		}
	};
} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{
	class LinkedListTest : public TestCollection
	{
	public:
		LinkedListTest() : TestCollection("LinkedListTest") {}

	protected:
		void Prepare() override;
	};
} // namespace hbe
#endif //__UNIT_TEST__
