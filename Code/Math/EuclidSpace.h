// Copyright Hansol Park (mooming.go@gmail.com). All rights reserved.

#ifndef JointTransform_h
#define JointTransform_h

#include "AffineTransform.h"
#include "Vector3.h"
#include "Quaternion.h"

#include "System/Debug.h"
#include "System/String.h"
#include "System/Optional.h"
#include "System/Vector.h"

namespace HE
{
    template <typename Number>
    class EuclidSpace final
    {
        using This = EuclidSpace;
        using Transform = AffineTransform<Number>;
        using Vec3 = Vector3<Number>;
        using Quat = Quaternion<Number>;
        using Mat4x4 = Matrix4x4<Number>;

    private:
        Transform local;
        Optional<Transform> world;

        This* pParent;
        Vector<This*> pChildren;

    public:
        EuclidSpace() : local(), world(nullptr), pParent(nullptr), pChildren()
        {
        }

        ~EuclidSpace()
        {
        }

        const Transform& Local() const
        {
            return local;
        }

        const Transform& World()
        {
            if (pParent == nullptr)
                return local;

            if (world)
            {
                return *world;
            }

            world = pParent->World() * local;

            return *world;
        }

        EuclidSpace* Parent() const
        {
            return pParent;
        }

        void SetParent(EuclidSpace* pParent)
        {
            This::pParent = pParent;
            pChildren.Add(this);
            
            Invalidate();
        }
        
        void SetLocalPosition(const Vec3& position)
        {
            local.SetTranslation(position);
            Invalidate();
        }
        
        void SetLocalRotation(const Quat& rotation)
        {
            local.SetRotation(rotation);
            Invalidate();
        }
        
        void SetLocalScale(const Vec3& scale)
        {
            local.SetScale(scale);
            Invalidate();
        }
        
        void SetLocalRotationScale(const Quat& rotation, const Vec3& scale)
        {
            local.SetRotationScale(rotation, scale);
            Invalidate();
        }
        
        void SetLocalTRS(const Vec3& t, const Quat& r, const Vec3& s)
        {
            local.SetTRS(t, r, s);
            Invalidate();
        }
        
        void SetPosition(const Vec3& position)
        {
            auto pos = pParent != nullptr
                ? pParent->World().InverseTransform(position)
                : position;
            
            local.SetTranslation(pos);
            Invalidate();
        }
        
        void SetRotation(const Quat& rotation)
        {
            const auto rot = pParent == nullptr
                ? rotation
                : pParent->World().Rotation().Inverse() * rotation;
            
            local.SetRotation(rot);
            Invalidate();
        }
        
        void SetScale(const Vec3& scale)
        {
            const auto s = pParent == nullptr
                ? scale
                : scale / pParent->World().Scale();
            
            local.SetScale(s);
            Invalidate();
        }
        
        void SetRotationScale(const Quat& rotation, const Vec3& scale)
        {
            if (pParent == nullptr)
            {
                local.SetRotationScale(rotation, scale);
            }
            else
            {
                auto r = pParent->World().Rotation().Inverse() * rotation;
                auto s = scale / pParent->World().Scale();
                local.SetRotationScale(r, s);
            }
            
            Invalidate();
        }
        
        void SetTRS(const Vec3& t, const Quat& r, const Vec3& s)
        {
            if (pParent == nullptr)
            {
                local.SetTRS(t, r, s);
            }
            else
            {
                auto nt = pParent->World().InverseTransform(t);
                auto nr = pParent->World().Rotation().Inverse() * r;
                auto ns = s / pParent->World().Scale();
                local.SetTRS(nt, nr, ns);
            }
            
            Invalidate();
        }

    private:
        void Invalidate()
        {
            if (!world)
                return;
            
            world = nullptr;

            for(auto& child : pChildren)
            {
                child->Invalidate();
            }
        }
    };

    using FloatSpace = EuclidSpace<float>;

    template <typename T>
    std::ostream& operator<< (std::ostream& os, EuclidSpace<T>& space)
    {
        using namespace std;
        
        os << "EuclidSpace ###" << endl;
        os << "Local = " << space.Local() << endl;
        os << "World = " << space.World() << endl;
        
        return os;
    }
}

#ifdef __UNIT_TEST__

#include "System/TestCase.h"

namespace HE
{
    class EuclidSpaceTest : public TestCase
    {
    public:
        EuclidSpaceTest() : TestCase("EuclidSpaceTest")
        {
        }

    protected:
        virtual bool DoTest() override;
    };
}
#endif //__UNIT_TEST__

#endif //JointTransform_h
