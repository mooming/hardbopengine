// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "Config/BuildConfig.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"

namespace hbe::Renderer
{

/// @brief Position + color vertex used by the renderer's simple mesh path.
struct Vertex
{
	Vector3<float> position;
	Vector4<float> color;

	Vertex() = default;

	Vertex(float x, float y, float z, float r, float g, float b, float a);
};

} // namespace hbe::Renderer
