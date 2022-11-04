// Created by mooming.go@gmail.com, 2017

#pragma once

#include "Memory/BaseAllocator.h"
#include "Memory/Memory.h"
#include "System/Debug.h"
#include <algorithm>
#include <initializer_list>
#include <memory>


namespace HE
{
    template <typename Element, class TAllocator = BaseAllocator<Element>>
    class Array
    {
    public:
        using TIndex = int;
        using Iterator = Element*;
        using ConstIterator = const Element*;

    private:
        TIndex length;
        Element* data;
        TAllocator allocator;

    public:
        Iterator begin() { return &data[0]; }
        Iterator end() { return &data[length]; }
        ConstIterator begin() const { return &data[0]; }
        ConstIterator end() const { return &data[length]; }

    public:
        Array(const Array&) = delete;
        Array& operator= (const Array&) = delete;

    public:
        Array()
            : length(0)
            , data(nullptr)
        {
        }

        explicit Array(TIndex size)
            : length(size)
        {
            data = allocator.allocate(length);
            for (TIndex i = 0; i < length; ++i)
            {
                new (&data[i]) Element();
            }
        }

        Array(std::initializer_list<Element> list)
            : Array(static_cast<TIndex>(list.size()))
        {
            TIndex index = 0;

            for (auto element : list)
            {
                data[index] = element;
                ++index;
            }
        }

        Array(Array&& rhs) : Array()
        {
            Swap(rhs);
        }

        virtual ~Array()
        {
            if (data == nullptr)
                return;

            for (auto& item : *this)
            {
                item.~Element();
            }
            
            allocator.deallocate(data, length);
        }

        Array& operator= (Array&& rhs)
        {
            Swap(rhs);
            return *this;
        }

        Element& operator[] (TIndex index)
        {
            FatalAssert(IsValidIndex(index));
            return data[index];
        }

        const Element& operator[] (TIndex index) const
        {
            FatalAssert(IsValidIndex(index));
            return data[index];
        }

        template <typename ... Types>
        Element& Emplace(TIndex index, Types&& ... args)
        {
            FatalAssert(IsValidIndex(index));

            auto& item = data[index];
            item.~Element();

            new (&item) Element(std::forward<Types>(args) ...);

            return item;
        }

        Element* ToRawArray()
        {
            return data;
        }

        const Element* const ToRawArray() const
        {
            return data;
        }

        TIndex Size() const
        {
            return length;
        }

        bool IsValidIndex(TIndex index) const
        {
            return index >= 0 && index < length;
        }

        void Clear()
        {
            Swap(Array());
        }

        void Swap(Array&& rhs)
        {
            auto tmpLength = length;
            auto tmpData = data;

            length = rhs.length;
            data = rhs.data;

            rhs.length = tmpLength;
            rhs.data = tmpData;
        }

        TIndex GetIndex(const Element& element) const
        {
            if (unlikely(data == nullptr))
                return -1;

            auto delta = static_cast<TIndex>(&element - &data[0]);

            return IsValidIndex(delta) ? delta : -1;
        }
    };

} // HE

#ifdef __UNIT_TEST__

#include "Test/TestCollection.h"


namespace HE
{

class ArrayTest : public TestCollection
{
public:
    ArrayTest() : TestCollection("ArrayTest")
    {
    }

protected:
    virtual void Prepare() override;
};

} // HE
    
#endif //__UNIT_TEST__
