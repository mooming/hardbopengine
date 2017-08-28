// Copyright Hansol Park (mooming.go@gmail.com). All rights reserved.

#ifndef JointTransform_h
#define JointTransform_h

#include "AffineTransform.h"

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
        using Mat4x4 = Matrix4x4<Number>;

    private:
        Transform local;
        Optional<Transform> world;

        This* parent;
        Vector<This*> children;

    public:
        EuclidSpace() : local(), world(nullptr), parent(nullptr), children()
        {
        }

        ~EuclidSpace()
        {
        }

        Transform Local() const
        {
            return local;
        }

        Transform World()
        {
            if (parent)
            {
                if (world)
                    return world;

                world = parent->World() * local;

                return world;
            }

            return local;
        }

        EuclidSpace* Parent() const
        {
            return parent;
        }

        void SetParent(EuclidSpace& parent)
        {
            This::parent = &parent;
            children.Add(this);
        }

    private:
        void Invalidate()
        {
            world = nullptr;

            for(auto& child : children)
            {
                Assert(child != nullptr);
                child->Invalidate();
            }
        }
    };

    using FloatSpace = EuclidSpace<float>;

    template <typename T>
    std::ostream& operator<< (std::ostream& os, const EuclidSpace<T>& joint)
    {
        using namespace std;

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
