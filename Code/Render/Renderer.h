#pragma once

#include "../System/Vector.h"

namespace HE
{
	class Renderable;

	class Renderer
	{
	private:
		Vector<Renderable> renderObjs;

	public:
		Renderer();

		void Render();
		void SetRenderTarget();
	};
}
