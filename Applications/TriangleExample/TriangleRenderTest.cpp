// Copyright (c) 2026 HardBop Engine. All rights reserved.

#include "DODTypes.h"
#include "OSAL/Window.h"
#include "SoftwareRenderer.h"
#include "Test/TestCollection.h"

#include <vector>

namespace hbe
{

// Simple mock window that captures SetPixels calls for verification.
class MockWindow final : public OS::IWindow
{
public:
	int width = 0;
	int height = 0;
	std::vector<uint32_t> pixels; // ARGB pixel buffer

	bool CreateWindow(const hbe::HString& /*title*/, int w, int h) override
	{
		width = w;
		height = h;
		return true;
	}

	void SetTitle(const hbe::HString&) override {}

	void SetSize(int w, int h) override
	{
		width = w;
		height = h;
	}

	void SetVisible(bool) override {}

	void PollEvents() override {}

	void Close() override {}

	int GetWidth() const override
	{
		return width;
	}

	int GetHeight() const override
	{
		return height;
	}

	bool IsVisible() const override
	{
		return false;
	}

	bool IsClosed() const override
	{
		return false;
	}

	intptr_t GetNativeHandle() const override
	{
		return 0;
	}

	void SetPixels(const uint32_t* p, int w, int h) override
	{
		width = w;
		height = h;
		pixels.assign(p, p + w * h);
	}
};

class TriangleRenderTest final : public TestCollection
{
public:
	TriangleRenderTest()
		: TestCollection("TriangleRenderTest")
	{}

protected:
	void Prepare() override
	{
		AddTest("BasicRender", [](TLogOut& log)
		{
			const int width = 64;
			const int height = 64;

			MockWindow mockWnd;
			// create a dummy window (size is set in Initialize)
			TriangleExample::SoftwareRenderer renderer;
			renderer.Initialize(&mockWnd, width, height);

			// Build a simple triangle covering part of the screen
			TriangleExample::PrepareBuffers buffers;
			TriangleExample::ProjectedTriangle tri;
			tri.v0 = {10.0f, 10.0f, 0.0f};
			tri.v1 = {50.0f, 10.0f, 0.0f};
			tri.v2 = {30.0f, 40.0f, 0.0f};
			tri.color = {1.0f, 1.0f, 1.0f, 1.0f};
			// Compute area for barycentric weights
			float area = (tri.v1.x - tri.v0.x) * (tri.v2.y - tri.v0.y) - (tri.v1.y - tri.v0.y) * (tri.v2.x - tri.v0.x);
			tri.invArea = (std::abs(area) > 1e-6f) ? (1.0f / area) : 0.0f;
			buffers.triangles.push_back(tri);

			renderer.Render(buffers);

			// Verify that at least one pixel differs from the clear colour (0xFF101010)
			bool anyDrawn = false;
			for (uint32_t px : mockWnd.pixels)
			{
				if (px != 0xFF101010u)
				{
					anyDrawn = true;
					break;
				}
			}
			if (anyDrawn)
			{
				log << "Render produced non‑clear pixels – success";
			}
			else
			{
				log << "Render produced only clear pixels – failure";
			}
		});
	}
};

} // namespace hbe
