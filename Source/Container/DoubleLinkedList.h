// Created by mooming.go@gmail.com, 2017

#pragma once

#include "Allocator.h"
#include "Debug.h"

#include <memory>

namespace HE
{
template <typename Type>
class DoubleLinkedList
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
        Node* next;

    public:
        Iterator(Node* node) : node(node), next(node ? node->next : nullptr) {}

        inline Iterator& operator++ ()
        {
            node = next;
            next = node ? node->next : nullptr;

            return *this;
        }

        inline bool operator != (const Iterator& rhs) const
        {
            return node != rhs.node;
        }

        inline Type& operator* () { return node->value; }
        inline const Type& operator* () const { return node->value; }
    };

    using ConstIterator = Iterator;

public:
    Iterator begin() { return Iterator(head); }
    Iterator end() { return Iterator(nullptr); }
    ConstIterator begin() const { return Iterator(head); }
    ConstIterator end() const { return Iterator(nullptr); }

public:
    DoubleLinkedList(const DoubleLinkedList&) = delete;
    DoubleLinkedList& operator= (const DoubleLinkedList&) = delete;

public:
    DoubleLinkedList() : head(nullptr), tail(nullptr), size(0)
    {
    }

    DoubleLinkedList(DoubleLinkedList&& rhs) : head(rhs.head), tail(rhs.tail), size(rhs.size)
    {
        rhs.head = nullptr;
        rhs.tail = nullptr;
    }

    DoubleLinkedList& operator= (DoubleLinkedList&& rhs)
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

    ~DoubleLinkedList()
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
        for (auto& element : *this)
        {
            Remove(element);
        }
    }

    inline void Remove(Type& element) { Delete(Unlink(GetNodeOf(element))); }
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
        return AddFirst(New<Node>(value))->value;
    }

    inline Type& AddFirst(Type&& value)
    {
        return AddFirst(New<Node>(std::forward<Type&&>(value)))->value;
    }

    inline Type& AddLast(const Type& value)
    {
        return AddLast(New<Node>(value))->value;
    }

    inline Type& AddLast(Type&& value)
    {
        return AddLast(New<Node>(std::forward<Type&&>(value)))->value;
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
    inline Node* GetNodeOf(Type& element)
    {
        Assert(ContainsElement(element));
        return reinterpret_cast<Node*>(&element);
    }

    inline Node* AddFirst(Node* node) { return AddPrevious(head, node); }
    inline Node* AddLast(Node* node) { return AddNext(tail, node); }

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
        }

        return node;
    }

private:
    inline void LinkPrevious(Node* node, Node* newNode)
    {
        Assert(node != nullptr);
        Assert(newNode != nullptr);

        if (node->IsHead())
        {
            head = newNode;
        }
        else
        {
            Node* previous = node->previous;
            previous->next = newNode;
            newNode->previous = previous;
        }

        node->previous = newNode;
        newNode->next = node;

        ++size;
    }

    inline void LinkNext(Node* node, Node* newNode)
    {
        Assert(node != nullptr);
        Assert(newNode != nullptr);

        if (node->IsTail())
        {
            tail = newNode;
        }
        else
        {
            Node* next = node->next;
            next->previous = newNode;;
            newNode->next = next;
        }

        node->next = newNode;
        newNode->previous = node;

        ++size;
    }

    inline Node* Unlink(Node* node)
    {
        Assert(node != nullptr);
        --size;

        Node* previous = node->previous;
        Node* next = node->next;

        if (node->IsHead())
        {
            head = next;
            if (head)
            {
                head->previous = nullptr;
            }
        }
        else if (node->IsTail())
        {
            tail = previous;
            if (tail)
            {
                tail->next = nullptr;
            }
        }
        else
        {
            previous->next = next;
            next->previous = previous;
        }

        node->previous = nullptr;
        node->next = nullptr;

        return node;
    }
};

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

class DoubleLinkedListTest : public TestCollection
{
public:
    DoubleLinkedListTest() : TestCollection("DoubleLinkedListTest") {}

protected:
    virtual void Prepare() override;
};
#endif __UNIT_TEST__

} // HE
