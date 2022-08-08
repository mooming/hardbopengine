// Created by mooming.go@gmail.com, 2017

#pragma once

#include "MemoryManager.h"


namespace HE
{
    template <typename Type, typename RefCount = uint8_t>
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
            template <typename ... Types>
            static Body* Create(Types&& ... args)
            {
                auto& mmgr = MemoryManager::GetInstance();
                auto newBody = mmgr.New<Body>(std::forward<Types>(args) ...);
                return newBody->Reference();
            }

            inline Type& GetBody()
            {
                return data;
            }

            inline Body* Reference()
            {
                ++count;
                return this;
            }

            inline RefCount GetRefCount() const
            {
                return count;
            }

            inline void Dereference()
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

            template <typename ... Types>
            inline Body(Types&& ... args) : count(0), data(std::forward<Types>(args) ...)
            {
            }
        };

    private:
        Body* body;

    public:
        template <typename ... Types>
        inline Shareable(Types&& ... args) : body(Body::Create(std::forward<Types>(args) ...))
        {
        }

        inline Shareable(Shareable& rhs)
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

        inline Shareable(Shareable&& rhs)
        {
            body = rhs.body;
            rhs.body = nullptr;
        }

        inline ~Shareable()
        {
            if (body != nullptr)
            {
                body->Dereference();
                body = nullptr;
            }
        }

        inline Shareable& operator= (Shareable& rhs)
        {
            Release();

            if (rhs)
            {
                body = rhs.body->Reference();
            }
        }

        inline RefCount GetReferenceCount() const
        {
            return body != nullptr ? body->GetRefCount() : 0;
        }

        inline operator bool() const
        {
            return body != nullptr;
        }

        inline Type& Get()
        {
            return body->GetBody();
        }

        inline const Type& Get() const
        {
            return body->GetBody();
        }

        inline Type& operator* ()
        {
            return body->GetBody();
        }

        inline const Type& operator* () const
        {
            return body->GetBody();
        }

        inline Type* operator-> ()
        {
            return &body->GetBody();
        }

        inline const Type* operator-> () const
        {
            return &body->GetBody();
        }

        inline void Release()
        {
            if (body != nullptr)
            {
                body->Dereference();
                body = nullptr;
            }
        }

        inline void Swap(Shareable& rhs)
        {
            auto tmpBody = body;
            body = rhs.body;
            rhs.body = tmpBody;
        }
    };
} // HE
