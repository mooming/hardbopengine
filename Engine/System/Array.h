// Created by mooming.go@gmail.com, 2017

#pragma once

#include "Allocator.h"
#include "Types.h"

#include <algorithm>
#include <initializer_list>

namespace HE
{
    template <typename Element>
    class Array
    {
        using Iterator = Element*;
        using ConstIterator = const Element*;

    private:
        Index length;
        Element* data;

    public:
        inline Iterator begin() { return &data[0]; }
        inline Iterator end() { return &data[length]; }
        inline ConstIterator begin() const { return &data[0]; }
        inline ConstIterator end() const { return &data[length]; }

    public:
        inline Element& operator[] (Index index) { return data[index]; }
        inline const Element& operator[] (Index index) const { return data[index]; }

    public:
        inline Array() : length(0), data(nullptr)
        {
        }

        explicit inline Array(Index size) : length(size)
        {
            constexpr auto sizeOfElement = sizeof(Element);
            data = Allocate<Element>(sizeOfElement * size);
        }

        inline Array(std::initializer_list<Element> list)
            : Array(static_cast<Index>(list.size()))
        {
            Index index = 0;
            for (auto element : list)
            {
                data[index] = element;
                ++index;
            }
        }

        inline Array(Array&& rhs) : Array()
        {
            Swap(rhs);
        }

        inline virtual ~Array()
        {
            if (data != nullptr)
                Deallocate(data);
        }

        inline Array& operator= (const Array& rhs) = delete;

        inline Array& operator= (Array&& rhs)
        {
            Swap(rhs);
            return *this;
        }

        inline Element* ToRawArray()
        {
            return data;
        }

        inline const Element* const ToRawArray() const
        {
            return data;
        }

        inline Index Length() const
        {
            return length;
        }

        inline Index Size() const
        {
            return length;
        }

        inline void Destroy()
        {
            Swap(Array());
        }

        inline Array Clone()
        {
            return Array(*this);
        }

        inline void Swap(Array&& rhs)
        {
            auto tmpLength = length;
            auto tmpData = data;

            length = rhs.length;
            data = rhs.data;

            rhs.length = tmpLength;
            rhs.data = tmpData;
        }

        inline void Resize(Index size)
        {
            Array tmp(size);
            const Index shorterLen = std::min(size, length);

            constexpr auto sizeOfElement = sizeof(Element);
            memcpy(tmp.data, data, sizeOfElement * shorterLen);

            Swap(std::move(tmp));
        }

        inline Index GetIndex(const Element& element) const
        {
            AssertMessage(data != nullptr, "The array is null.\n");
            const Index delta = static_cast<Index>(&element - &data[0]);
            AssertMessage(delta >= 0, "The given element is not an element of it. %p / %p\n", &element, &data[0]);
            return delta;
        }

    private:

        inline Array(const Array& rhs) : Array(rhs.length)
        {
            memcpy(data, rhs.data, length);
        }
    };

#ifdef __UNIT_TEST__
    class ArrayTest : public TestCase
    {
    public:
        ArrayTest() : TestCase("ArrayTest") {}

    protected:
        virtual bool DoTest() override;
    };
#endif //__UNIT_TEST__

}
