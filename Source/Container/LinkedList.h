// Created by mooming.go@gmail.com, 2017

#pragma once

#include "Memory/BaseAllocator.h"
#include "Memory/Memory.h"
#include "System/Debug.h"
#include <functional>

namespace hbe
{
    template <typename Type>
    struct LinkedListNode final
    {
        using This = LinkedListNode;
        Type value;
        LinkedListNode* previous;
        LinkedListNode* next;

        inline LinkedListNode(const Type& value)
            : value(value),
              previous(nullptr),
              next(nullptr)
        {
        }

        inline LinkedListNode(Type&& value)
            : value(std::move(value)),
              previous(nullptr),
              next(nullptr)
        {
        }

        inline bool operator!=(const This& rhs) const { return this != &rhs; }
        inline Type& operator*() { return value; }
        inline const Type& operator*() const { return value; }
        inline bool IsHead() const { return previous == nullptr; }
        inline bool IsTail() const { return next == nullptr; }
    };

    template <typename Type,
        class TAllocator = BaseAllocator<LinkedListNode<Type>>>
    class LinkedList final
    {
    public:
        using Node = LinkedListNode<Type>;

    public:
        class Iterator
        {
        private:
            Node* node;

        public:
            inline Iterator(Node* node)
                : node(node)
            {
            }
            inline void operator++() { node = node->next; }
            inline bool operator!=(const Iterator& rhs) const
            {
                return node != rhs.node;
            }
            inline Type& operator*() { return node->value; }
            inline const Type& operator*() const { return node->value; }
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
        LinkedList()
            : head(nullptr),
              tail(nullptr)
        {
        }

        LinkedList(LinkedList&& rhs)
            : head(rhs.head),
              tail(rhs.tail)
        {
            rhs.head = nullptr;
            rhs.tail = nullptr;
        }

        LinkedList& operator=(LinkedList&& rhs)
        {
            Node* tmpHead = rhs.head;
            Node* tmpTail = rhs.tail;

            rhs.head = nullptr;
            rhs.tail = nullptr;

            head = tmpHead;
            tail = tmpTail;

            return *this;
        }

        ~LinkedList() { Clear(); }

    public:
        Iterator begin() { return Iterator(head); }
        Iterator end() { return Iterator(nullptr); }
        ConstIterator begin() const { return ConstIterator(head); }
        ConstIterator end() const { return ConstIterator(nullptr); }

    public:
        bool IsEmpty() const
        {
            Assert(head != nullptr || head == tail);
            return head == nullptr;
        }

        void Clear()
        {
            while (head != nullptr)
            {
                RemoveNode(head);
            }
        }

        Iterator Remove(const Type& element)
        {
            Assert(ContainsElement(element));
            return Iterator(RemoveNode(GetNodeOf(element)));
        }

        Type& Add(const Type& value) { return AddLast(value); }

        Type& Add(Type&& value) { return AddLast(std::move(value)); }

        bool Contains(const Type& value) const
        {
            for (auto& element : *this)
            {
                if (element == value)
                {
                    return true;
                }
            }

            return false;
        }

        bool Contains(const Type* ptr) const
        {
            for (auto& element : *this)
            {
                if (&element == ptr)
                {
                    return true;
                }
            }

            return false;
        }

        Type* Find(const Type& value)
        {
            for (auto& element : *this)
            {
                if (element == value)
                {
                    return &element;
                }
            }

            return nullptr;
        }

        const Type* Find(const Type& value) const
        {
            for (auto& element : *this)
            {
                if (element == value)
                {
                    return &element;
                }
            }

            return nullptr;
        }

        Index Count(const Type& value) const
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

        bool FindAndRemove(const Type& value)
        {
            if (auto found = Find(value))
            {
                Remove(*found);
                return true;
            }

            return false;
        }

    public:
        inline Type& AddFirst(const Type& value)
        {
            return AddPrevious(head, New<Node>(allocator, value))->value;
        }

        inline Type& AddFirst(Type&& value)
        {
            return AddPrevious(
                head, New<Node>(allocator, std::forward<Type&&>(value)))
                ->value;
        }

        inline Type& AddLast(const Type& value)
        {
            return AddNext(tail, New<Node>(allocator, value))->value;
        }

        inline Type& AddLast(Type&& value)
        {
            return AddNext(
                tail, New<Node>(allocator, std::forward<Type&&>(value)))
                ->value;
        }

        inline Type& AddPrevious(Type& current, const Type& value)
        {
            return AddPrevious(GetNodeOf(current), New<Node>(allocator, value))
                ->value;
        }

        inline Type& AddPrevious(Type& current, Type&& value)
        {
            return AddPrevious(GetNodeOf(current),
                New<Node>(allocator, std::forward<Type&&>(value)))
                ->value;
        }

        inline Type& AddNext(Type& current, const Type& value)
        {
            return AddNext(GetNodeOf(current), New<Node>(allocator, value))
                ->value;
        }

        inline Type& AddNext(Type& current, Type&& value)
        {
            return AddNext(
                GetNodeOf(current), New<Node>(allocator, std::forward(value)))
                ->value;
        }

    private:
        Node* RemoveNode(Node* node)
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

        inline Node* GetNodeOf(Type& element)
        {
            Assert(ContainsElement(element));
            return reinterpret_cast<Node*>(&element);
        }

        inline Node* AddPrevious(Node* current, Node* node)
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

        inline Node* AddNext(Node* current, Node* node)
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
        inline void LinkPrevious(Node* node, Node* newNode)
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

        inline void LinkNext(Node* node, Node* newNode)
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

        inline void Unlink(Node* node)
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
        LinkedListTest()
            : TestCollection("LinkedListTest")
        {
        }

    protected:
        virtual void Prepare() override;
    };
} // namespace hbe
#endif //__UNIT_TEST__
