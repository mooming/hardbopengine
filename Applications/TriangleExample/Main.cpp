// TriangleExample - minimal Vulkan triangle demo using OSAL window

// OSAL removed for minimal stub example
#include "MinimalVulkanRenderer.h"

#include <atomic>
#include <csignal>
#include <iostream>

using namespace hbe;

static std::atomic<bool> running{true};

void signalHandler(int) {
    running = false;
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // No window/application in this stub

    MinimalVulkanRenderer renderer;
    if (!renderer.Initialize(nullptr)) {
        std::cerr << "Failed to initialize renderer" << std::endl;
        return 1;
    }

    // Run a few frames of the stub renderer then exit
    for (int i = 0; i < 10; ++i) {
        renderer.BeginFrame();
        renderer.Render(0.016f); // simulate ~60 FPS
        renderer.EndFrame();
    }
    renderer.Shutdown();
    return 0;
}
