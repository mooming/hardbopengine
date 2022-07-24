#include "Renderer.h"

namespace HE
{
	Renderer::Renderer()
	{
		Open();
	}

	Renderer::~Renderer()
	{
		Close();
	}

	void Renderer::Render()
	{
	}

	Renderable& Renderer::Create()
	{
		return renderObjs[0];
	}

	void Renderer::Remove(Renderable & renderable)
	{
	}
}
