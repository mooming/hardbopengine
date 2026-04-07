// Created by mooming.go@gmail.com

#pragma once

#include "MemoryManager.h"

namespace hbe
{
	/// @brief Reference-counted smart pointer for shared ownership.
	/// @details Manages a heap-allocated object with reference counting.
	/// Automatically deallocates when reference count reaches zero.
	template<typename Type, typename RefCount = uint8_t>
	class Shareable
	{
	private:
		class Body
		{
		private:
			static constexpr auto SizeOfType = sizeof(Type);
			RefCount count;
			Type data;

		public:
			template<typename... Types>
			static Body* Create(Types&&... args)
			{
				auto& mmgr = MemoryManager::GetInstance();
				auto newBody = mmgr.New<Body>(std::forward<Types>(args)...);
				return newBody->Reference();
			}

			Type& GetBody() { return data; }

			Body* Reference()
			{
				++count;
				return this;
			}

			RefCount GetRefCount() const { return count; }

			void Dereference()
			{
				if (count > 0)
				{
					--count;

					if (count == 0)
					{
						auto& mmgr = MemoryManager::GetInstance();
						mmgr.Delete<Body>(this);
					}
				}
			}

			template<typename... Types>
			Body(Types&&... args) : count(0), data(std::forward<Types>(args)...)
			{}
		};

	private:
		Body* body;

	public:
		template<typename... Types>
		Shareable(Types&&... args) : body(Body::Create(std::forward<Types>(args)...))
		{}

		Shareable(Shareable& rhs)
		{
			if (rhs)
			{
				body = rhs.body->Reference();
			}
			else
			{
				body = nullptr;
			}
		}

		Shareable(Shareable&& rhs)
		{
			body = rhs.body;
			rhs.body = nullptr;
		}

		~Shareable()
		{
			if (body != nullptr)
			{
				body->Dereference();
				body = nullptr;
			}
		}

		Shareable& operator=(Shareable& rhs)
		{
			Release();

			if (rhs)
			{
				body = rhs.body->Reference();
			}
		}

		RefCount GetReferenceCount() const { return body != nullptr ? body->GetRefCount() : 0; }

		operator bool() const { return body != nullptr; }

		Type& Get() { return body->GetBody(); }

		const Type& Get() const { return body->GetBody(); }

		Type& operator*() { return body->GetBody(); }

		const Type& operator*() const { return body->GetBody(); }

		Type* operator->() { return &body->GetBody(); }

		const Type* operator->() const { return &body->GetBody(); }

		void Release()
		{
			if (body != nullptr)
			{
				body->Dereference();
				body = nullptr;
			}
		}

		void Swap(Shareable& rhs)
		{
			auto tmpBody = body;
			body = rhs.body;
			rhs.body = tmpBody;
		}
	};
} // namespace hbe
