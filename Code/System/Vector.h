// Copyright Hansol Park (anav96@naver.com, mooming.go@gmail.com). All rights reserved.

#ifndef Vector_h
#define Vector_h

#include "Allocator.h"
#include "Debug.h"

#include <algorithm>
#include <initializer_list>

namespace HE
{
    template <typename Element>
    class Vector
    {
        using Iterator = Element*;
        using ConstIterator = const Element*;

    public:
        Vector& operator= (const Vector& rhs) = delete;

    private:
        Index capacity;
        Index size;
        Element* data;

    public:
        inline Iterator begin() { return &data[0]; }
        inline Iterator end() { return &data[size]; }
        inline ConstIterator begin() const { return &data[0]; }
        inline ConstIterator end() const { return &data[size]; }

    public:
        inline Element& operator[] (Index index)
        {
            Assert(index < size);
            return data[index];
        }

        inline const Element& operator[] (Index index) const
        {
            Assert(index < size);
            return data[index];
        }

    public:
        inline Vector() : capacity(0), size(0), data(nullptr)
        {
        }

        inline explicit Vector(Index capacity) : capacity(capacity), size(0), data(nullptr)
        {
            constexpr auto sizeOfElement = sizeof(Element);
            data = Allocate<Element>(sizeOfElement * capacity);
        }

        inline ~Vector()
        {
            Clear();

            if (data != nullptr)
            {
                Deallocate(data);
            }
        }

        inline Vector& operator= (Vector&& rhs)
        {
            Swap(rhs);
            return *this;
        }

    public:
        inline void Add(const Element& value)
        {
            if (size >= capacity)
            {
                Reserve((size + 1) * 3 / 2);
            }

            data[size++] = value;
        }

        inline void Add(Element&& value)
        {
            if (size >= capacity)
            {
                Reserve((size + 1) * 3 / 2);
            }

            auto& element = data[size++];
            Construct<Element>(&element, std::move(value));
        }

        template <typename ... Types>
        inline Element& New(Types&& ... args)
        {
            if (size >= capacity)
            {
                Reserve((size + 1) * 3 / 2);
            }

            auto& element = data[size++];
            Construct<Element>(&element, std::forward<Types>(args) ...);

            return element;
        }

        inline void RemoveAt(const Element& value)
        {
            const auto index = GetIndex(value);
            Assert(index < size);

            data[index].~Element();

            const auto indexDiff = size - index - 1;
            constexpr auto sizeOfElement = sizeof(Element);
            memcpy((data + index), (data + index + 1), sizeOfElement * indexDiff);
        }

        inline void Remove(const Element& value)
        {
            const auto index = GetIndex(value);

            if (index >= 0 && index < size)
            {
                RemoveAt(value);
                return;
            }

            for (auto& element : *this)
            {
                if (element == value)
                {
                    RemoveAt(element);
                    break;
                }
            }
        }

        inline void RemoveAll(const Element& value)
        {
            for (auto& element : data)
            {
                if (element == value)
                {
                    RemoveAt(element);
                }
            }
        }

        inline void Fit()
        {
            Reserve(size);
        }

        inline void Fill()
        {
            Resize(capacity);
        }

        inline void Clear()
        {
            if (data != nullptr)
            {
                for (Index i = 0; i < size; ++i)
                {
                    data[i].~Element();
                }

                size = 0;
            }
        }

        inline Index Size() const
        {
            return size;
        }

        inline bool IsEmpty() const
        {
            return size == 0;
        }

        inline Index Capacity() const
        {
            return capacity;
        }

        inline Element* ToRawArray()
        {
            return data;
        }

        inline const Element* const ToRawArray() const
        {
            return data;
        }

        inline Vector Clone() const
        {
            return Vector(*this);
        }

        inline void Swap(Vector&& rhs)
        {
            auto tmpCapacity = capacity;
            auto tmpSize = size;
            auto tmpData = data;

            capacity = rhs.capacity;
            size = rhs.size;
            data = rhs.data;

            rhs.capacity = tmpCapacity;
            rhs.size = tmpSize;
            rhs.data = tmpData;
        }

        inline void Reserve(Index newCapacity)
        {
            constexpr auto sizeOfElement = sizeof(Element);

            Vector tmp(newCapacity);
            const auto newSize = std::min(newCapacity, size);
            tmp.size = newSize;

            memcpy((void*)tmp.data, (void*)data, sizeOfElement * newSize);

            for (Index i = newSize; i < size; ++i)
            {
                data[i].~Element();
            }

            size = 0;

            Swap(std::move(tmp));
        }

        inline void Resize(Index newSize)
        {
            if (capacity < newSize)
            {
                Reserve(newSize);
            }

            for (Index i = newSize; i < size; ++i)
            {
                data[i].~Element();
            }

            for (Index i = size; i < newSize; ++i)
            {
                Construct<Element>(&data[i]);
            }

            size = newSize;
        }

        inline void ChangeSize(Index newSize)
        {
            if (capacity < newSize)
            {
                Reserve(newSize);
            }

            size = newSize;
        }

        inline Index GetIndex(const Element& element) const
        {
            AssertMessage(data != nullptr, "vector is null.\n");
            const Index delta = static_cast<Index>(&element - &data[0]);
            AssertMessage(delta >= 0, "The given element is not an element of it. %p / %p\n", &element, &data[0]);
            return delta;
        }

    private:
        inline Vector(const Vector& rhs) : Vector(rhs.size)
        {
            constexpr auto sizeOfElement = sizeof(Element);
            size = rhs.size;
            memcpy(data, rhs.data, sizeOfElement * size);
        }
    };
}

#ifdef __UNIT_TEST__
namespace HE
{
    class VectorTest : public TestCase
    {
    public:
        VectorTest() : TestCase("VectorTest") {}

    protected:
        virtual bool DoTest() override;
    };
}
#endif //__UNIT_TEST__

#endif //Vector_h
