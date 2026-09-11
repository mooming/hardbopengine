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
		bool isHead() const noexcept { return previous == nullptr; }
		bool isTail() const noexcept { return next == nullptr; }
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

		~LinkedList() noexcept { clear(); }

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

		void clear() noexcept
		{
			while (head != nullptr)
			{
				removeNode(head);
			}
		}

		Iterator Remove(const TType& element) noexcept
		{
			Assert(ContainsElement(element));
			return Iterator(removeNode(getNodeOf(element)));
		}

		TType& add(const TType& value) noexcept { return addLast(value); }

		TType& add(TType&& value) noexcept { return addLast(std::move(value)); }

		[[nodiscard]] bool contains(const TType& value) const noexcept
		{
			for (auto& element : *this)
			{
				if (element == value)
					return true;
			}

			return false;
		}

		[[nodiscard]] bool contains(const TType* ptr) const noexcept
		{
			for (auto& element : *this)
			{
				if (&element == ptr)
					return true;
			}

			return false;
		}

		[[nodiscard]] TType* find(const TType& value) noexcept
		{
			for (auto& element : *this)
			{
				if (element == value)
					return &element;
			}

			return nullptr;
		}

		[[nodiscard]] const TType* find(const TType& value) const noexcept
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

		bool findAndRemove(const TType& value) noexcept
		{
			if (auto found = find(value))
			{
				Remove(*found);
				return true;
			}

			return false;
		}

	public:
		TType& addFirst(const TType& value) noexcept { return addPrevious(head, New<Node>(allocator, value))->value; }

		TType& addFirst(TType&& value) noexcept
		{
			return addPrevious(head, New<Node>(allocator, std::forward<TType&&>(value)))->value;
		}

		TType& addLast(const TType& value) noexcept { return addNext(tail, New<Node>(allocator, value))->value; }

		TType& addLast(TType&& value) noexcept
		{
			return addNext(tail, New<Node>(allocator, std::forward<TType&&>(value)))->value;
		}

		TType& addPrevious(TType& current, const TType& value) noexcept
		{
			return addPrevious(getNodeOf(current), New<Node>(allocator, value))->value;
		}

		TType& addPrevious(TType& current, TType&& value) noexcept
		{
			return addPrevious(getNodeOf(current), New<Node>(allocator, std::forward<TType&&>(value)))->value;
		}

		TType& addNext(TType& current, const TType& value) noexcept
		{
			return addNext(getNodeOf(current), New<Node>(allocator, value))->value;
		}

		TType& addNext(TType& current, TType&& value) noexcept
		{
			return addNext(getNodeOf(current), New<Node>(allocator, std::forward(value)))->value;
		}

	private:
		Node* removeNode(Node* node) noexcept
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

		Node* getNodeOf(TType& element) noexcept
		{
			Assert(ContainsElement(element));
			return reinterpret_cast<Node*>(&element);
		}

		Node* addPrevious(Node* current, Node* node) noexcept
		{
			Assert((current != nullptr || IsEmpty()) && node != nullptr);

			if (IsEmpty())
			{
				head = node;
				tail = node;
			}
			else
			{
				linkPrevious(current, node);
				if (current == head)
				{
					head = node;
				}
			}

			return node;
		}

		Node* addNext(Node* current, Node* node) noexcept
		{
			Assert((current != nullptr || IsEmpty()) && node != nullptr);

			if (IsEmpty())
			{
				head = node;
				tail = node;
			}
			else
			{
				linkNext(current, node);
				if (current == tail)
				{
					tail = node;
				}
			}

			return node;
		}

	private:
		void linkPrevious(Node* node, Node* newNode) noexcept
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

		void linkNext(Node* node, Node* newNode) noexcept
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
		void prepare() override;
	};
} // namespace hbe
#endif //__UNIT_TEST__
