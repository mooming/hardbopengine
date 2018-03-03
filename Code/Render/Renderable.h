#pragma once

namespace HE
{
	class Material;
	class Mesh;

	class Renderable
	{
	public:
		Material* GetMaterial();
		Mesh* GetMesh();

	};
}
