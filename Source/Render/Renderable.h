#pragma once

#include "../Math/Matrix4x4.h"

namespace HE
{
	class Material;
	class Mesh;
	class GameObject;
	class Renderer;

	class Renderable
	{
	private:
		GameObject& gameObject;

	public:
		Renderable(GameObject& gameObject);
		virtual ~Renderable();

		Material* GetMaterial();
		Mesh* GetMesh();
		Float4x4 GetWorldMatrix();
	};
}
