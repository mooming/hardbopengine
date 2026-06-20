// Copyright (c) 2026 HardBop Engine. All rights reserved.
#pragma once

#include <cstdint>
#include <vector>
#include "DODTypes.h"
#include "Engine/OSAL/Window.h"

namespace hbe
{
namespace TriangleExample
{

class SoftwareRenderer final
{
public:
	SoftwareRenderer() noexcept = default;
	~SoftwareRenderer() = default;

	void Initialize(OS::IWindow* inWindow, int inWidth, int inHeight);

	void Render(const PrepareBuffers& inPrepareBuffers);

public:
	void RenderTile(int inTileX, int inTileY, int inWidth, int inHeight, const PrepareBuffers& inPrepareBuffers);
	void WritePixel(int x, int y, float w0, float w1, float w2, const Math::TFloat4& color, int inWidth);

	OS::IWindow* window;
	int width;
	int height;
	std::vector<uint32_t> frameBuffer;

	static constexpr int TileSize = 32;
};

} // namespace TriangleExample
} // namespace hbe
