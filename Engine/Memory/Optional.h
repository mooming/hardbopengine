// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "Core/CommonUtil.h"
#include "Core/Debug.h"
#include "Core/Types.h"
#include "OSAL/OSMemory.h"

namespace hbe
{
	/// @brief Stack-allocated optional value similar to std::optional.
	/// @details Stores a value or null without heap allocation.
	template<typename TType>
	class Optional final
	{
	public:
		bool hasValue;

		alignas(std::max(IsReferenceType<TType>::TypeSize,
						 Config::DefaultAlign)) Byte value[IsReferenceType<TType>::TypeSize];

	public:
		Optional() : hasValue(false) {}

		Optional(const TType& value) : hasValue(true) { Value() = value; }

		Optional(const Optional& rhs)
		{
			if (rhs.hasValue)
			{
				CopyValue(typename IsReferenceType<TType>::Result(), rhs);
			}
			else if (hasValue)
			{
				Destroy(typename IsReferenceType<TType>::Result());
			}
		}

		Optional(Optional&& rhs)
		{
			if (rhs.hasValue)
			{
				MoveValue(typename IsReferenceType<TType>::Result(), rhs);
			}
			else
			{
				Destroy(typename IsReferenceType<TType>::Result());
			}
		}

		Optional(std::nullptr_t) : hasValue(false) {}

		~Optional() { Destroy(typename IsReferenceType<TType>::Result()); }

		Optional& operator=(TType& value)
		{
			if (hasValue)
			{
				Value() = value;
				return *this;
			}

			Emplace(value);

			return *this;
		}

		Optional& operator=(const Optional& rhs)
		{
			if (hasValue)
			{
				Destroy(typename IsReferenceType<TType>::Result());
			}

			if (rhs.hasValue)
			{
				CopyValue(typename IsReferenceType<TType>::Result(), rhs);
			}

			return *this;
		}

		Optional& operator=(Optional&& rhs)
		{
			if (hasValue)
			{
				Destroy(typename IsReferenceType<TType>::Result());
			}

			if (rhs.hasValue)
			{
				MoveValue(typename IsReferenceType<TType>::Result(), rhs);
			}

			return *this;
		}

		Optional& operator=(std::nullptr_t)
		{
			Reset();

			return *this;
		}

		[[nodiscard]] auto HasValue() const noexcept { return hasValue; }
		[[nodiscard]] operator bool() const noexcept { return hasValue; }

		TType& operator*()
		{
			FatalAssert(hasValue);
			return Value();
		}

		const TType& operator*() const
		{
			FatalAssert(hasValue);
			return Value();
		}

		[[nodiscard]] TType& Value() { return GetValue(typename IsReferenceType<TType>::Result()); }

		[[nodiscard]] const TType& Value() const { return GetValue(typename IsReferenceType<TType>::Result()); }

		void Reset()
		{
			if (!hasValue)
			{
				return;
			}

			Destroy(typename IsReferenceType<TType>::Result());
		}

		void Emplace(TType& value)
		{
			if (hasValue)
			{
				Destroy(typename IsReferenceType<TType>::Result());
			}

			ConstructAt(typename IsReferenceType<TType>::Result(), value);
		}

		template<typename... Types>
		void Emplace(Types&&... args)
		{
			if (hasValue)
			{
				Destroy(typename IsReferenceType<TType>::Result());
			}

			ConstructAt(typename IsReferenceType<TType>::Result(), std::forward<Types>(args)...);
		}

	private:
		void ConstructAt(True_t, TType& inValue)
		{
			using TValue = typename std::decay<TType>::type;
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

		template<typename... Types>
		void ConstructAt(False_t, Types&&... args)
		{
			hasValue = true;
			new (value) TType(std::forward<Types>(args)...);
		}

		void CopyValue(True_t, const Optional& rhs)
		{
			hasValue = rhs.hasValue;
			memcpy(value, rhs.value, sizeof(TType));
		}

		void CopyValue(False_t, const Optional& rhs)
		{
			hasValue = rhs.hasValue;
			Value() = rhs.Value();
		}

		void MoveValue(True_t, Optional& rhs)
		{
			hasValue = rhs.hasValue;
			memcpy(value, rhs.value, sizeof(TType));
			rhs.hasValue = false;
		}

		void MoveValue(False_t, Optional& rhs)
		{
			hasValue = rhs.hasValue;
			Value() = std::move(rhs.Value());
			rhs.hasValue = false;
		}

		TType& GetValue(True_t)
		{
			using TValue = typename std::decay<TType>::type;
			using TypePtr = TValue*;
			TypePtr& valuePtr = reinterpret_cast<TypePtr&>(value[0]);
			Assert(valuePtr != nullptr);

			return *valuePtr;
		}

		TType& GetValue(False_t) { return reinterpret_cast<TType&>(value[0]); }

		const TType& GetValue(True_t) const
		{
			using TValue = typename std::decay<TType>::type;
			using TypePtr = TValue*;
			TypePtr& valuePtr = reinterpret_cast<TypePtr&>(value[0]);
			Assert(valuePtr != nullptr);

			return *valuePtr;
		}

		const TType& GetValue(False_t) const { return reinterpret_cast<const TType&>(value[0]); }

		void Destroy(True_t) { hasValue = false; }

		void Destroy(False_t)
		{
			if (hasValue)
			{
				Value().~TType();
			}

			hasValue = false;
		}
	};
} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{
	class OptionalTest : public TestCollection
	{
	public:
		OptionalTest() : TestCollection("OptionalTest") {}

	protected:
		void Prepare() override;
	};

} // namespace hbe

#endif //__UNIT_TEST__
