#pragma once

#include "UniformTransform.h"

#include "../System/Optional.h"
#include "../System/StdUtil.h"
#include "../System/Vector.h"

namespace HE
{
	template<typename Number>
	class Transform
	{
	public:
		using This = Transform;
		using Vec3 = Vector3<Number>;
		using Quat = Quaternion<Number>;
		using Mat3x3 = Matrix3x3<Number>;
		using Mat4x4 = Matrix4x4<Number>;
		using UTransform = UniformTransform<Number>;

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
		Transform& operator== (const Transform&) = delete;

	public:
		Transform() : trs(), parent(nullptr), children()
		{
		}

        Transform(std::nullptr_t) : trs(nullptr), parent(nullptr), children()
		{
		}

        Transform(Transform& parent) : trs(), parent(nullptr), children()
        {
            parent.Attach(*this);
        }

        Transform(const UTransform& trs) : trs(trs), parent(nullptr), children()
        {
        }

		void Attach(Transform& transform)
		{
			auto ptr = &transform;
			if (std::find(children.begin(), children.end(), ptr) != children.end())
			{
				AssertMessage(transform.parent == this, "Parent-Child Inconsistency");
				return;
			}

            children.Add(ptr);
            transform.parent = this;
		}

        Transform* GetParent() const { return parent; }

		bool Detach(Transform& transform)
		{
			auto ptr = &transform;
			for (auto it = children.begin(); it != children.end(); ++it)
			{
				auto child = *it;
				if (child == ptr)
				{
					it = children.erase(it);
					AssertMessage(child->parent == this, "Parent-Child Inconsistency");
					child->parent = nullptr;

					return true;
				}
			}

			return false;
		}

		bool HasChild(const Transform& child) const
		{
            auto ptr = &child;
			return std::find(children.cbegin(), children.cend(), ptr) != children.cend();
		}

		std::vector<Transform*>& GetChildren() { return children; }
		const std::vector<Transform*>& GetChildren() const { return children; }

		void Invalidate()
		{
            if (!world)
                return;
            
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
				return *world;

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
                return *world;

            if (parent)
            {
                return parent->GetWorldTransform() * trs;
            }
            
            return trs;
        }

		const UTransform& GetLocalTransform() const { return trs; }

		void Set(const Quat& r, Number s, const Vec3& t)
		{
			if (rotation == r && Equals(scale, s) && translation == t)
				return;

			rotation = r;
			scale = s;
			translation = t;

			Invalidate();
		}

		void Set(const Quat& r)
		{
			if (rotation == r)
				return;

			rotation = r;

			Invalidate();
		}

		void Set(Number s)
		{
			if (scale == s)
				return;

			scale = s;

			Invalidate();
		}

		void Set(const Vec3& t)
		{
			if (translation == t)
				return;

			translation = t;

			Invalidate();
		}
	};

    using FTransform = Transform<float>;
    using DTransform = Transform<double>;

    template <typename T>
    inline std::ostream& operator<<(std::ostream& os, const Transform<T>& t)
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

} // HE

#ifdef __UNIT_TEST__

#include "System/TestCase.h"

namespace HE
{

    class TransformTest : public TestCase
    {
    public:

        TransformTest() : TestCase("TransformTest")
        {
        }

    protected:
        virtual bool DoTest() override;
    };
}

#endif //__UNIT_TEST__
