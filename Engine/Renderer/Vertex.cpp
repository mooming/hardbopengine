// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Vertex.h"

namespace hbe::Renderer
{

Vertex::Vertex(float x, float y, float z, float r, float g, float b, float a)
	: position(x, y, z)
	, color(r, g, b, a)
{
}

} // namespace hbe::Renderer
