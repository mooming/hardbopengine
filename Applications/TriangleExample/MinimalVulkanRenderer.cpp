#include "MinimalVulkanRenderer.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace hbe
{

MinimalVulkanRenderer::~MinimalVulkanRenderer()
{
	Shutdown();
}

bool MinimalVulkanRenderer::Initialize(OS::IWindow* /*window*/) noexcept
{
	// Allocate framebuffer
	framebuffer.resize(width * height);
	std::cout << "MinimalVulkanRenderer initialized (software renderer)" << std::endl;
	return true;
}

void MinimalVulkanRenderer::Shutdown() noexcept
{
	// Write final frame if any
	if (!framebuffer.empty())
	{
		// Ensure last frame saved
	}
	std::cout << "MinimalVulkanRenderer shutdown" << std::endl;
}

void MinimalVulkanRenderer::BeginFrame() noexcept
{
	// Clear to dark gray
	std::fill(framebuffer.begin(), framebuffer.end(), 0xFF202020);
}

void MinimalVulkanRenderer::EndFrame() noexcept
{
	// Save framebuffer as PPM image for each frame
	std::ostringstream filename;
	filename << "frame_" << std::setfill('0') << std::setw(3) << frameIndex++ << ".ppm";
	std::ofstream out(filename.str(), std::ios::binary);
	if (!out)
	{
		std::cerr << "Failed to write frame " << filename.str() << std::endl;
		return;
	}

	// PPM header (binary P6)
	out << "P6\n" << width << " " << height << "\n255\n";
	// Write pixel data (RGB, ignoring alpha)
	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			uint32_t pixel = framebuffer[y * width + x];
			unsigned char r = (pixel >> 16) & 0xFF;
			unsigned char g = (pixel >> 8) & 0xFF;
			unsigned char b = pixel & 0xFF;
			out.put(r).put(g).put(b);
		}
	}
	out.close();
}

// Simple barycentric triangle rasterizer
static void drawTriangle(std::vector<uint32_t>& buf, int w, int h, float x0, float y0, float x1, float y1, float x2,
						 float y2, uint32_t color)
{
	// Bounding box
	int minX = static_cast<int>(std::floor(std::min({x0, x1, x2})));
	int maxX = static_cast<int>(std::ceil(std::max({x0, x1, x2})));
	int minY = static_cast<int>(std::floor(std::min({y0, y1, y2})));
	int maxY = static_cast<int>(std::ceil(std::max({y0, y1, y2})));
	// Clamp
	minX = std::max(minX, 0);
	maxX = std::min(maxX, w - 1);
	minY = std::max(minY, 0);
	maxY = std::min(maxY, h - 1);
	// Edge function
	auto edge = [&](float ax, float ay, float bx, float by, float px, float py)
	{ return (px - ax) * (by - ay) - (py - ay) * (bx - ax); };
	for (int py = minY; py <= maxY; ++py)
	{
		for (int px = minX; px <= maxX; ++px)
		{
			float w0 = edge(x1, y1, x2, y2, px + 0.5f, py + 0.5f);
			float w1 = edge(x2, y2, x0, y0, px + 0.5f, py + 0.5f);
			float w2 = edge(x0, y0, x1, y1, px + 0.5f, py + 0.5f);
			if (w0 >= 0 && w1 >= 0 && w2 >= 0)
			{
				buf[py * w + px] = color;
			}
		}
	}
}

void MinimalVulkanRenderer::Render(float /*deltaTime*/) noexcept
{
	// Draw a static colored triangle in the center
	float cx = width / 2.0f;
	float cy = height / 2.0f;
	float size = std::min(width, height) * 0.4f;
	// Define vertices
	float x0 = cx;
	float y0 = cy - size / 2.0f;
	float x1 = cx - size / 2.0f;
	float y1 = cy + size / 2.0f;
	float x2 = cx + size / 2.0f;
	float y2 = cy + size / 2.0f;
	uint32_t triColor = 0xFF00FF00; // ARGB green
	drawTriangle(framebuffer, width, height, x0, y0, x1, y1, x2, y2, triColor);
	std::cout << "Rendered triangle to framebuffer" << std::endl;
}

} // namespace hbe
