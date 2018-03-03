#pragma once

#include <cstdint>

namespace HE
{
	class RenderTarget
	{
	private:
		int width;
		int height;
		uint8_t colorDepth;

	public:
		RenderTarget();
	};
}
