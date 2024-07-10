// Created by mooming.go@gmail.com, 2017

#pragma once

#include "UniformTransform.h"

#include "HSTL/HVector.h"
#include "Log/Logger.h"
#include "MathUtil.h"
#include "Memory/Optional.h"
#include <ostream>


namespace HE
{
template <typename Number>
class Transform final
{
  public:
    using This = Transform;
    using Vec3 = Vector3<Number>;
    using Quat = Quaternion<Number>;
    using Mat3x3 = Matrix3x3<Number>;
    using Mat4x4 = Matrix4x4<Number>;
    using UTransform = UniformTransform<Number>;

    template <class T>
    using Vector = HSTL::HVector<T>;

  public:
    union
    {
        UTransform trs;
        struct
        {
            Quat rotation;
            Number scale;
            Vec3 translation;
        };
    };

  private:
    Optional<UTransform> world;

    Transform* parent;
    Vector<Transform*> children;

  public:
    Transform(const Transform&) = delete;
    Transform& operator==(const Transform&) = delete;

    Transform() : trs(), parent(nullptr) {}

    Transform(std::nullptr_t) : trs(nullptr), parent(nullptr) {}

    Transform(const UTransform& trs) : trs(trs), parent(nullptr) {}

    ~Transform()
    {
        if (parent)
        {
            parent->Detach(*this);
        }

        DetachAll();
    }

    void Attach(Transform& transform)
    {
        auto ptr = &transform;

        if (ptr == parent)
        {
            parent->Detach(*this);
        }

        if (std::find(children.begin(), children.end(), ptr) != children.end())
        {
            Assert(transform.parent == this, "Parent-Child Inconsistency");
            return;
        }

        children.push_back(ptr);
        transform.parent = this;
    }

    Transform* GetParent() const { return parent; }

    void Detach(Transform& transform)
    {
        if (transform.parent != this)
        {
            return;
        }

        transform.parent = nullptr;

        auto ptr = &transform;
        auto it = std::find(children.begin(), children.end(), ptr);
        Assert(it != children.end());

        children.erase(it);
    }

    void DetachAll()
    {
        for (auto child : children)
        {
            Assert(child->parent == this, "Parent-Child Inconsistency");
            child->parent = nullptr;
        }

        children.clear();
    }

    bool HasChild(const Transform& child) const
    {
        auto ptr = &child;
        bool bHasChild = std::find(children.cbegin(), children.cend(), ptr) != children.cend();

        return bHasChild;
    }

    Vector<Transform*>& GetChildren() { return children; }
    const Vector<Transform*>& GetChildren() const { return children; }

    void Invalidate()
    {
        if (!world)
        {
            return;
        }

        world = nullptr;
        for (auto child : children)
        {
            Assert(child);
            child->Invalidate();
        }
    }

    const UTransform& GetWorldTransform()
    {
        if (world)
        {
            return *world;
        }

        if (parent)
        {
            world = parent->GetWorldTransform() * trs;
            return *world;
        }

        return trs;
    }

    UTransform GetWorldTransform() const
    {
        if (world)
        {
            return *world;
        }

        if (parent)
        {
            return parent->GetWorldTransform() * trs;
        }

        return trs;
    }

    const UTransform& GetLocalTransform() const { return trs; }

    void Set(const Quat& r, Number s, const Vec3& t)
    {
        if (rotation == r && IsEqual(scale, s) && translation == t)
        {
            return;
        }

        rotation = r;
        scale = s;
        translation = t;

        Invalidate();
    }

    void Set(const Quat& r)
    {
        if (rotation == r)
        {
            return;
        }

        rotation = r;

        Invalidate();
    }

    void Set(Number s)
    {
        if (scale == s)
        {
            return;
        }

        scale = s;

        Invalidate();
    }

    void Set(const Vec3& t)
    {
        if (translation == t)
        {
            return;
        }

        translation = t;

        Invalidate();
    }
};

using FTransform = Transform<float>;
using DTransform = Transform<double>;

template <typename T>
std::ostream& operator<<(std::ostream& os, const Transform<T>& t)
{
    using namespace std;
    os << "Transform" << endl;
    os << t.trs << endl;

    auto parent = t.GetParent();
    os << "Parent: " << parent << endl;

    if (parent)
    {
        os << "World Transform" << endl;
        os << t.GetWorldTransform() << endl;
    }

    return os;
}

template <typename T>
LogStream& operator<<(LogStream& os, const Transform<T>& t)
{
    os << "Transform" << hendl;
    os << t.trs << hendl;

    auto parent = t.GetParent();
    os << "Parent: " << parent << hendl;

    if (parent)
    {
        os << "World Transform" << hendl;
        os << t.GetWorldTransform() << hendl;
    }

    return os;
}

} // namespace HE

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace HE
{
class TransformTest : public TestCollection
{
  public:
    TransformTest() : TestCollection("TransformTest") {}

  protected:
    virtual void Prepare() override;
};
} // namespace HE
#endif //__UNIT_TEST__
