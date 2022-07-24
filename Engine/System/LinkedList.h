// Created by mooming.go@gmail.com, 2017

#pragma once

#include "Allocator.h"
#include "Debug.h"
#include "TestCase.h"

#include <functional>
#include <memory>

namespace HE
{
    template <typename Type>
    class LinkedList
    {
    private:
        struct Node
        {
            Type value;
            Node* previous;
            Node* next;

            inline Node(const Type& value) : value(value)
                , previous(nullptr), next(nullptr)
            {
            }

            inline Node(Type&& value) : value(std::move(value))
                , previous(nullptr), next(nullptr)
            {
            }

            inline bool operator != (const Node& rhs) const { return this != &rhs; }

            inline Type& operator* () { return value; }
            inline const Type& operator* () const { return value; }

            inline bool IsHead() const { return previous == nullptr; }
            inline bool IsTail() const { return next == nullptr; }
        };

    private:
        Node* head;
        Node* tail;
        Index size;

    public:
        class Iterator
        {
        private:
            Node* node;

        public:
            inline Iterator(Node* node) : node(node) {}
            inline void operator++ () { node = node->next; }
            inline bool operator != (const Iterator& rhs) const { return node != rhs.node; }
            inline Type& operator* () { return node->value; }
            inline const Type& operator* () const { return node->value; }
        };

        using ConstIterator = Iterator;

    public:
        Iterator begin() { return Iterator(head); }
        Iterator end() { return Iterator(nullptr); }
        ConstIterator begin() const { return ConstIterator(head); }
        ConstIterator end() const { return ConstIterator(nullptr); }

    public:
        LinkedList(const LinkedList&) = delete;
        LinkedList& operator= (const LinkedList&) = delete;

    public:
        LinkedList() : head(nullptr), tail(nullptr), size(0)
        {
        }

        LinkedList(LinkedList&& rhs) : head(rhs.head), tail(rhs.tail), size(rhs.size)
        {
            rhs.head = nullptr;
            rhs.tail = nullptr;
        }

        LinkedList& operator= (LinkedList&& rhs)
        {
            Node* tmpHead = rhs.head;
            Node* tmpTail = rhs.tail;
            Index tmpSize = rhs.size;

            rhs.head = nullptr;
            rhs.tail = nullptr;
            rhs.size = 0;

            head = tmpHead;
            tail = tmpTail;
            size = tmpSize;

            return *this;
        }

        ~LinkedList()
        {
            Clear();
        }

    public:
        inline Index Size() const { return size; }
        inline bool IsEmpty() const
        {
            Assert(head != nullptr || head == tail);
            return head == nullptr;
        }

    public:
        inline void Clear()
        {
            while (head != nullptr)
            {
                RemoveNode(head);
            }
        }

        inline Iterator Remove(Type& element)
        {
            Assert(ContainsElement(element));
            return Iterator(RemoveNode(GetNodeOf(element)));
        }

        inline Type& Add(const Type& value) { return AddLast(value); }
        inline Type& Add(Type&& value) { return AddLast(std::move(value)); }

        bool Contains(Type value) const
        {
            for (auto& element : *this)
            {
                if (element == value)
                    return true;
            }

            return false;
        }

        bool ContainsElement(Type& value) const
        {
            for (auto& element : *this)
            {
                if (&element == &value)
                    return true;
            }

            return false;
        }

        Type* Find(Type value)
        {
            for (auto& element : *this)
            {
                if (element == value)
                    return &element;
            }

            return nullptr;
        }

        const Type* Find(Type value) const
        {
            for (auto& element : *this)
            {
                if (element == value)
                    return &element;
            }

            return nullptr;
        }

        Index Count(const Type& value) const
        {
            int count = 0;
            for (auto& element : *this)
            {
                if (element == value)
                    ++count;
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
            return AddPrevious(head, New<Node>(value))->value;
        }

        inline Type& AddFirst(Type&& value)
        {
            return AddPrevious(head, New<Node>(std::forward<Type&&>(value)))->value;
        }

        inline Type& AddLast(const Type& value)
        {
            return AddNext(tail, New<Node>(value))->value;
        }

        inline Type& AddLast(Type&& value)
        {
            return AddNext(tail, New<Node>(std::forward<Type&&>(value)))->value;
        }

        inline Type& AddPrevious(Type& current, const Type& value)
        {
            return AddPrevious(GetNodeOf(current), New<Node>(value))->value;
        }

        inline Type& AddPrevious(Type& current, Type&& value)
        {
            return AddPrevious(GetNodeOf(current), New<Node>(std::forward<Type&&>(value)))->value;
        }

        inline Type& AddNext(Type& current, const Type& value)
        {
            return AddNext(GetNodeOf(current), New<Node>(value))->value;
        }

        inline Type& AddNext(Type& current, Type&& value)
        {
            return AddNext(GetNodeOf(current), New<Node>(std::forward(value)))->value;
        }

    private:
        Node *RemoveNode(Node* node)
        {
            auto next = node->next;

            if (node == head)
                head = next;
            else if (node == tail)
                tail = node->previous;

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
                    head = node;
            }

            ++size;
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
                    tail = node;
            }

            ++size;
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

            if (node->previous == nullptr && node->next == nullptr)
                return;

            Node* prev = node->previous;
            Node* next = node->next;

            if (prev && next)
            {
                prev->next = next;
                next->previous = prev;
            }
            else if (prev)
            {
                prev->next = nullptr;
            }
            else
            {
                next->previous = nullptr;
            }

            Delete(node);
            --size;
        }
    };
}

#ifdef __UNIT_TEST__
#include "TestCase.h"
namespace HE
{
    class LinkedListTest : public TestCase
    {
    public:
        LinkedListTest() : TestCase("LinkedListTest") {}

    protected:
        virtual bool DoTest() override;
    };
}
#endif //__UNIT_TEST__
