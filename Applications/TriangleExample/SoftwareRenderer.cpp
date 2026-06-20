// Copyright (c) 2026 HardBop Engine. All rights reserved.

#include "SoftwareRenderer.h"
#include <algorithm>
#include <arm_neon.h>
#include <cmath>
#include "Engine/Core/Task.h"
#include "Engine/Core/TaskSystem.h"

namespace hbe
{
namespace TriangleExample
{

void SoftwareRenderer::Initialize(OS::IWindow* inWindow, int inWidth, int inHeight)
{
	window = inWindow;
	width = inWidth;
	height = inHeight;
	frameBuffer.assign(width * height, 0xFF000000);
}

struct RenderTaskData
{
	SoftwareRenderer* renderer;
	const PrepareBuffers* prepareBuffers;
	int numTilesX;
};

std::size_t RasterizeTask(void* userData, std::size_t startIndex, std::size_t endIndex)
{
	auto* data = static_cast<RenderTaskData*>(userData);
	auto* renderer = data->renderer;

	for (std::size_t i = startIndex; i < endIndex; ++i)
	{
		int tileX = (i % data->numTilesX) * SoftwareRenderer::TileSize;
		int tileY = (i / data->numTilesX) * SoftwareRenderer::TileSize;
		renderer->RenderTile(tileX, tileY, renderer->width, renderer->height, *data->prepareBuffers);
	}

	return endIndex - startIndex;
}

void SoftwareRenderer::Render(const PrepareBuffers& inPrepareBuffers)
{
	if (!window)
		return;

	// Clear framebuffer
	std::fill(frameBuffer.begin(), frameBuffer.end(), 0xFF101010);

	int numTilesX = (width + TileSize - 1) / TileSize;
	int numTilesY = (height + TileSize - 1) / TileSize;
	auto totalTiles = static_cast<std::size_t>(numTilesX * numTilesY);

	RenderTaskData taskData{this, &inPrepareBuffers, numTilesX};
	Task renderTask("RasterizeTiles", RasterizeTask, &taskData);

	auto numThreads = static_cast<uint8_t>(hbe::TaskSystem::GetNumHardwareThreads());
	renderTask.Start(numThreads, 0, totalTiles, 0);
	renderTask.Wait();

	window->SetPixels(frameBuffer.data(), width, height);
}

void SoftwareRenderer::RenderTile(int inTileX, int inTileY, int inWidth, int inHeight,
								  const PrepareBuffers& inPrepareBuffers)
{
	int startX = std::max(0, inTileX);
	int startY = std::max(0, inTileY);
	int endX = std::min(inWidth, inTileX + TileSize);
	int endY = std::min(inHeight, inTileY + TileSize);

	for (const auto& tri : inPrepareBuffers.triangles)
	{
		float minX = std::min({tri.v0.x, tri.v1.x, tri.v2.x});
		float maxX = std::max({tri.v0.x, tri.v1.x, tri.v2.x});
		float minY = std::min({tri.v0.y, tri.v1.y, tri.v2.y});
		float maxY = std::max({tri.v0.y, tri.v1.y, tri.v2.y});

		int clipStartX = std::max(startX, static_cast<int>(std::floor(minX)));
		int clipEndX = std::min(endX, static_cast<int>(std::ceil(maxX)));
		int clipStartY = std::max(startY, static_cast<int>(std::floor(minY)));
		int clipEndY = std::min(endY, static_cast<int>(std::ceil(maxY)));

		if (clipStartX >= clipEndX || clipStartY >= clipEndY)
			continue;

		float a0 = tri.v1.y - tri.v2.y;
		float b0 = tri.v2.x - tri.v1.x;
		float c0 = tri.v1.x * tri.v2.y - tri.v1.y * tri.v2.x;

		float a1 = tri.v2.y - tri.v0.y;
		float b1 = tri.v0.x - tri.v2.x;
		float c1 = tri.v2.x * tri.v0.y - tri.v2.y * tri.v0.x;

		float a2 = tri.v0.y - tri.v1.y;
		float b2 = tri.v1.x - tri.v0.x;
		float c2 = tri.v0.x * tri.v1.y - tri.v0.y * tri.v1.x;

		float invArea = tri.invArea;

		float32x4_t va0 = vdupq_n_f32(a0);
		float32x4_t va1 = vdupq_n_f32(a1);
		float32x4_t va2 = vdupq_n_f32(a2);
		// float32x4_t vInvArea = vdupq_n_f32(invArea); // unused, kept for potential SIMD optimization

		for (int y = clipStartY; y < clipEndY; ++y)
		{
			float py = static_cast<float>(y) + 0.5f;
			float32x4_t vb0 = vdupq_n_f32(b0 * py + c0);
			float32x4_t vb1 = vdupq_n_f32(b1 * py + c1);
			float32x4_t vb2 = vdupq_n_f32(b2 * py + c2);

			int x = clipStartX;
			for (; x <= clipEndX - 4; x += 4)
			{
				float32x4_t vpx = {static_cast<float>(x) + 0.5f, static_cast<float>(x + 1) + 0.5f,
								   static_cast<float>(x + 2) + 0.5f, static_cast<float>(x + 3) + 0.5f};

				float32x4_t vw0 = vmlaq_f32(vb0, vpx, va0);
				float32x4_t vw1 = vmlaq_f32(vb1, vpx, va1);
				float32x4_t vw2 = vmlaq_f32(vb2, vpx, va2);

				uint32x4_t m0 = vcgeq_f32(vw0, vdupq_n_f32(0.0f));
				uint32x4_t m1 = vcgeq_f32(vw1, vdupq_n_f32(0.0f));
				uint32x4_t m2 = vcgeq_f32(vw2, vdupq_n_f32(0.0f));
				uint32x4_t finalMask = vandq_u32(vandq_u32(m0, m1), m2);

				if (vgetq_lane_u32(finalMask, 0))
				{
					float w0 = vgetq_lane_f32(vw0, 0) * invArea;
					float w1 = vgetq_lane_f32(vw1, 0) * invArea;
					float w2 = 1.0f - w0 - w1;
					WritePixel(x, y, w0, w1, w2, tri.color, inWidth);
				}
				if (vgetq_lane_u32(finalMask, 1))
				{
					float w0 = vgetq_lane_f32(vw0, 1) * invArea;
					float w1 = vgetq_lane_f32(vw1, 1) * invArea;
					float w2 = 1.0f - w0 - w1;
					WritePixel(x + 1, y, w0, w1, w2, tri.color, inWidth);
				}
				if (vgetq_lane_u32(finalMask, 2))
				{
					float w0 = vgetq_lane_f32(vw0, 2) * invArea;
					float w1 = vgetq_lane_f32(vw1, 2) * invArea;
					float w2 = 1.0f - w0 - w1;
					WritePixel(x + 2, y, w0, w1, w2, tri.color, inWidth);
				}
				if (vgetq_lane_u32(finalMask, 3))
				{
					float w0 = vgetq_lane_f32(vw0, 3) * invArea;
					float w1 = vgetq_lane_f32(vw1, 3) * invArea;
					float w2 = 1.0f - w0 - w1;
					WritePixel(x + 3, y, w0, w1, w2, tri.color, inWidth);
				}
			}
			for (; x < clipEndX; ++x)
			{
				float px = static_cast<float>(x) + 0.5f;
				float w0 = a0 * px + b0 * py + c0;
				float w1 = a1 * px + b1 * py + c1;
				float w2 = a2 * px + b2 * py + c2;
				if (w0 >= 0 && w1 >= 0 && w2 >= 0)
				{
					WritePixel(x, y, w0 * invArea, w1 * invArea, (1.0f - (w0 + w1) * invArea), tri.color, inWidth);
				}
			}
		}
	}
}

void SoftwareRenderer::WritePixel(int x, int y, float w0, float w1, float w2, const Math::TFloat4& color, int inWidth)
{
	// Simple flat color for the example triangle
	uint32_t pixelColor = (0xFF << 24) | ((uint32_t) (std::clamp(color.x, 0.0f, 1.0f) * 255.0f) << 16) |
						  ((uint32_t) (std::clamp(color.y, 0.0f, 1.0f) * 255.0f) << 8) |
						  ((uint32_t) (std::clamp(color.z, 0.0f, 1.0f) * 255.0f));

	frameBuffer[y * inWidth + x] = pixelColor;
}

} // namespace TriangleExample
} // namespace hbe
