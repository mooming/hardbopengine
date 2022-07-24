#pragma once

#include "Renderable.h"
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
		virtual ~Renderer();

		virtual void Render();

		Renderable& Create();
		void Remove(Renderable& renderable);

	protected:
		virtual void Open() = 0;
		virtual void Close() = 0;
	};
}
