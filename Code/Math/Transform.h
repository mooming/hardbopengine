#pragma once

#include "UniformTransform.h"

#include "../System/StdUtil.h"

#include <optional>
#include <vector>

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
			Quat rotation;
			Number scale;
			Vec3 translation;
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

		Transform(nullptr_t) : trs(nullptr), parent(nullptr), children()
		{
		}

		void Attach(Transform& transform)
		{
			auto ptr = &transform;
			if (std::find(children.cbegin(), children.cend(), ptr) != children.cend())
			{
				AssertMessage(child->parent == this, "Parent-Child Inconsistency");
				return;
			}

			children.push_back(ptr);
		}

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

		bool HasChild(const Transform& child)
		{
			return std::find(children.cbegin(), children.cend(), ptr) != children.cend();
		}

		std::vector<Transform*>& GetChildren() { return children; }
		const std::vector<Transform*>& GetChildren() const { return children; }

		void Invalidate()
		{
			world = nullptr;
			for (auto child : children)
			{
				Assert(child);
				child->Invalidate();
			}
		}

		const UTransform& GetWorldTransform() const
		{
			if (world)
				return world;

			if (parent)
				return parent->GetWorldTransform() * trs;

			return trs;
		}

		const UTransform& GetLocalTransform() const { return trs; }

		void Set(const Quaternion& r, Number s, const Vec3& t)
		{
			if (rotation == r && Equals(scale, s) && translation == t)
				return;

			rotation = r;
			scale = s;
			translation = t;

			Invalidate();
		}

		void Set(const Quaternion& r)
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

}
