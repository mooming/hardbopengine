// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "MemoryManager.h"

namespace hbe
{
	/// @brief Reference-counted smart pointer for shared ownership.
	/// @details Manages a heap-allocated object with reference counting.
	/// Automatically deallocates when reference count reaches zero.
	/// @brief Reference-counted smart pointer for shared ownership.
	/// @details Manages a heap-allocated object with reference counting.
	/// Automatically deallocates when reference count reaches zero.
	template<typename TType, typename RefCount = uint8_t>
	class Shareable
	{
	private:
		class Body
		{
		private:
			static constexpr auto SizeOfType = sizeof(TType);
			RefCount count;
			TType data;

		public:
			template<typename... Types>
			static Body* create(Types&&... args)
			{
				auto& mmgr = MemoryManager::getInstance();
				auto newBody = mmgr.New<Body>(std::forward<Types>(args)...);
				return newBody->reference();
			}

			TType& getBody() { return data; }

			Body* reference()
			{
				++count;
				return this;
			}

			RefCount getRefCount() const { return count; }

			void dereference()
			{
				if (count > 0)
				{
					--count;

					if (count == 0)
					{
						auto& mmgr = MemoryManager::getInstance();
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
		Shareable(Types&&... args) : body(Body::create(std::forward<Types>(args)...))
		{}

		Shareable(Shareable& rhs)
		{
			if (rhs)
			{
				body = rhs.body->reference();
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
				body->dereference();
				body = nullptr;
			}
		}

		Shareable& operator=(Shareable& rhs)
		{
			release();

			if (rhs)
			{
				body = rhs.body->reference();
			}
		}

		[[nodiscard]] RefCount getReferenceCount() const { return body != nullptr ? body->getRefCount() : 0; }

		operator bool() const { return body != nullptr; }

		[[nodiscard]] TType& get() { return body->getBody(); }

		[[nodiscard]] const TType& get() const { return body->getBody(); }

		[[nodiscard]] TType& operator*() { return body->getBody(); }

		[[nodiscard]] const TType& operator*() const { return body->getBody(); }

		[[nodiscard]] TType* operator->() { return &body->getBody(); }

		[[nodiscard]] const TType* operator->() const { return &body->getBody(); }

		void release()
		{
			if (body != nullptr)
			{
				body->dereference();
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
