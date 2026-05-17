// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <ostream>
#include "HSTL/HVector.h"
#include "Log/Logger.h"
#include "MathUtil.h"
#include "Memory/Optional.h"
#include "UniformTransform.h"

namespace hbe
{
	/// @brief A hierarchical transform with parent-child relationships and world transform caching.
	template<typename TNumber>
	class Transform final
	{
	public:
		using This = Transform;
		using TVec3 = Vector3<TNumber>;
		using TQuat = Quaternion<TNumber>;
		using TMat3x3 = Matrix3x3<TNumber>;
		using TMat4x4 = Matrix4x4<TNumber>;
		using TUTransform = UniformTransform<TNumber>;

		template<class T>
		using Vector = hbe::HVector<T>;

	public:
		union
		{
			TUTransform trs;
			struct
			{
				TQuat rotation;
				TNumber scale;
				TVec3 translation;
			};
		};

	private:
		Optional<TUTransform> world;

		Transform* parent;
		Vector<Transform*> children;

	public:
		Transform(const Transform&) = delete;
		Transform& operator==(const Transform&) = delete;

		Transform() noexcept : trs(), parent(nullptr) {}

		explicit Transform(std::nullptr_t) noexcept : trs(nullptr), parent(nullptr) {}

		explicit Transform(const TUTransform& trs) noexcept : trs(trs), parent(nullptr) {}

		~Transform() noexcept
		{
			if (parent)
			{
				parent->Detach(*this);
			}

			DetachAll();
		}

		void Attach(Transform& transform) noexcept
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

		[[nodiscard]] Transform* GetParent() const noexcept { return parent; }

		void Detach(Transform& transform) noexcept
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

		void DetachAll() noexcept
		{
			for (auto child : children)
			{
				Assert(child->parent == this, "Parent-Child Inconsistency");
				child->parent = nullptr;
			}

			children.clear();
		}

		[[nodiscard]] bool HasChild(const Transform& child) const noexcept
		{
			auto ptr = &child;
			bool bHasChild = std::find(children.cbegin(), children.cend(), ptr) != children.cend();

			return bHasChild;
		}

		[[nodiscard]] Vector<Transform*>& GetChildren() noexcept { return children; }
		[[nodiscard]] const Vector<Transform*>& GetChildren() const noexcept { return children; }

		void Invalidate() noexcept
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

		[[nodiscard]] const TUTransform& GetWorldTransform() noexcept
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

		[[nodiscard]] TUTransform GetWorldTransform() const noexcept
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

		[[nodiscard]] const TUTransform& GetLocalTransform() const noexcept { return trs; }

		void Set(const TQuat& r, TNumber s, const TVec3& t) noexcept
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

		void Set(const TQuat& r) noexcept
		{
			if (rotation == r)
			{
				return;
			}

			rotation = r;

			Invalidate();
		}

		void Set(TNumber s) noexcept
		{
			if (scale == s)
			{
				return;
			}

			scale = s;

			Invalidate();
		}

		void Set(const TVec3& t) noexcept
		{
			if (translation == t)
			{
				return;
			}

			translation = t;

			Invalidate();
		}
	};

	using TFTransform = Transform<float>;
	using TDTransform = Transform<double>;

	template<typename T>
	std::ostream& operator<<(std::ostream& os, const Transform<T>& t) noexcept
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

	template<typename T>
	LogStream& operator<<(LogStream& os, const Transform<T>& t) noexcept
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

} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{
	class TransformTest final : public TestCollection
	{
	public:
		TransformTest() : TestCollection("TransformTest") {}

	protected:
		void Prepare() noexcept override;
	};
} // namespace hbe
#endif //__UNIT_TEST__
