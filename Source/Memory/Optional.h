// Created by mooming.go@gmail.com, 2017

#pragma once

#include "OSAL/OSMemory.h"
#include "System/CommonUtil.h"
#include "System/Debug.h"
#include "System/Types.h"
#include <algorithm>
#include <type_traits>
#include <utility>


namespace HE
{
template <typename Type>
class Optional final
{
public:
    bool hasValue;

    alignas(std::max(IsReferenceType<Type>::TypeSize, Config::DefaultAlign))
    Byte value[IsReferenceType<Type>::TypeSize];
    
public:
    Optional() : hasValue(false)
    {
    }
    
    Optional(const Type& value) : hasValue(true)
    {
        Value() = value;
    }
    
    Optional(const Optional& rhs)
    {
        if (rhs.hasValue)
        {
            CopyValue(typename IsReferenceType<Type>::Result(), rhs);
        }
        else if (hasValue)
        {
            Destroy(typename IsReferenceType<Type>::Result());
        }
    }
    
    Optional(Optional&& rhs)
    {
        if (rhs.hasValue)
        {
            MoveValue(typename IsReferenceType<Type>::Result(), rhs);
        }
        else
        {
            Destroy(typename IsReferenceType<Type>::Result());
        }
    }
    
    Optional(std::nullptr_t) : hasValue(false)
    {
    }
    
    ~Optional()
    {
        Destroy(typename IsReferenceType<Type>::Result());
    }
    
    Optional& operator= (Type& value)
    {
        if (hasValue)
        {
            Value() = value;
            return *this;
        }

        Emplace(value);

        return *this;
    }
    
    Optional& operator= (const Optional& rhs)
    {
        if (hasValue)
        {
            Destroy(typename IsReferenceType<Type>::Result());
        }
        
        if (rhs.hasValue)
        {
            CopyValue(typename IsReferenceType<Type>::Result(), rhs);
        }

        return *this;
    }
    
    Optional& operator= (Optional&& rhs)
    {
        if (hasValue)
        {
            Destroy(typename IsReferenceType<Type>::Result());
        }

        if (rhs.hasValue)
        {
            MoveValue(typename IsReferenceType<Type>::Result(), rhs);
        }

        return *this;
    }
    
    Optional& operator= (std::nullptr_t)
    {
        Reset();
        
        return *this;
    }

    auto HasValue() const { return hasValue; }
    operator bool() const { return hasValue; }

    Type& operator* ()
    {
        FatalAssert(hasValue);
        return Value();
    }

    const Type& operator* () const
    {
        FatalAssert(hasValue);
        return Value();
    }
    
    Type& Value()
    {
        return GetValue(typename IsReferenceType<Type>::Result());
    }
    
    const Type& Value() const
    {
        return GetValue(typename IsReferenceType<Type>::Result());
    }

    void Reset()
    {
        if (!hasValue)
            return;

        Destroy(typename IsReferenceType<Type>::Result());
    }

    void Emplace(Type& value)
    {
        if (hasValue)
        {
            Destroy(typename IsReferenceType<Type>::Result());
        }

        ConstructAt(typename IsReferenceType<Type>::Result(), value);
    }

    template <typename ... Types>
    void Emplace(Types&& ... args)
    {
        if (hasValue)
        {
            Destroy(typename IsReferenceType<Type>::Result());
        }

        ConstructAt(typename IsReferenceType<Type>::Result()
            , std::forward<Types>(args) ...);
    }
    
private:
    void ConstructAt(True_t, Type& inValue)
    {
        using TValue = typename std::decay<Type>::type;
        using TPtr = void*;

        TPtr inValuePtr = &inValue;
        Assert(inValuePtr != nullptr);

        TPtr& ptr = reinterpret_cast<TPtr&>(value[0]);
        ptr = inValuePtr;

        TValue& myValue = Value();
        TPtr valuePtr = &myValue;
        Assert(inValuePtr == valuePtr);

        hasValue = true;
    }

    template <typename ... Types>
    void ConstructAt(False_t, Types&& ... args)
    {
        hasValue = true;
        new (value) Type(std::forward<Types>(args) ...);
    }

    void CopyValue(True_t, const Optional& rhs)
    {
        hasValue = rhs.hasValue;
        memcpy(value, rhs.value, sizeof(Type));
    }

    void CopyValue(False_t, const Optional& rhs)
    {
        hasValue = rhs.hasValue;
        Value() = rhs.Value();
    }

    void MoveValue(True_t, Optional& rhs)
    {
        hasValue = rhs.hasValue;
        memcpy(value, rhs.value, sizeof(Type));
        rhs.hasValue = false;
    }

    void MoveValue(False_t, Optional& rhs)
    {
        hasValue = rhs.hasValue;
        Value() = std::move(rhs.Value());
        rhs.hasValue = false;
    }

    Type& GetValue(True_t)
    {
        using TValue = typename std::decay<Type>::type;
        using TypePtr = TValue*;
        TypePtr& valuePtr = reinterpret_cast<TypePtr&>(value[0]);
        Assert(valuePtr != nullptr);

        return *valuePtr;
    }

    Type& GetValue(False_t)
    {
        return reinterpret_cast<Type&>(value[0]);
    }

    const Type& GetValue(True_t) const
    {
        using TValue = typename std::decay<Type>::type;
        using TypePtr = TValue*;
        TypePtr& valuePtr = reinterpret_cast<TypePtr&>(value[0]);
        Assert(valuePtr != nullptr);

        return *valuePtr;
    }

    const Type& GetValue(False_t) const
    {
        return reinterpret_cast<const Type&>(value[0]);
    }

    void Destroy(True_t)
    {
        hasValue = false;
    }
    
    void Destroy(False_t)
    {
        if (hasValue)
        {
            Value().~Type();
        }
        
        hasValue = false;
    }
};
} // HE

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"


namespace HE
{
class OptionalTest : public TestCollection
{
public:
    OptionalTest() : TestCollection("OptionalTest")
    {
    }
    
protected:
    virtual void Prepare() override;
};
} // HE
#endif //__UNIT_TEST__
