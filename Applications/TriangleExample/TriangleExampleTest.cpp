// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.


#include <iostream>
#include "Renderer/RendererFactory.h"
#include "Renderer/IRenderer.h"
#include "OSAL/Window.h"
#include "Test/TestCollection.h"
#include "Test/TestEnv.h"

namespace hbe
{

class TriangleExampleTest final : public TestCollection
{
public:
	explicit TriangleExampleTest()
		: TestCollection("TriangleExampleTest")
	{}

protected:
	void Prepare() noexcept override
	{
		// Test: ensure the engine's renderer can be created via the factory
		AddTest("InitializeRenderer", [](TLogOut& log)
		{
			auto renderer = hbe::Renderer::RendererFactory::Create();
			bool ok = renderer && renderer->Initialize(nullptr);
			if (ok)
			{
				log << "Renderer initialized successfully";
			}
			else
			{
				log << "Renderer initialization failed";
			}
		});
	}
};

// Register the test collection with the global TestEnv
static void RegisterTriangleExampleTest()
{
	TestEnv::GetEnv().AddTestCollection<TriangleExampleTest>();
}

// Ensure registration runs before main
static int dummy = (RegisterTriangleExampleTest(), 0);

} // namespace hbe
