// Created by mooming.go@gmail.com

#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>
#include <cstring>

enum class WindowMode
{
    Window,
    Fullscreen,
    FullscreenWindowed
};

struct TriangleVertex
{
    float pos[3];
    float color[3];
};

const TriangleVertex vertices[] = {
    {{0.0f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
    {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}}
};

const char* vertexShaderSrc = R"(
#version 450
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 0) out vec3 outColor;
void main() {
    gl_Position = vec4(inPosition, 1.0);
    outColor = inColor;
}
)";

const char* fragmentShaderSrc = R"(
#version 450
layout(location = 0) in vec3 inColor;
layout(location = 0) out vec4 outColor;
void main() {
    outColor = vec4(inColor, 1.0);
}
)";

int main(int argc, char* argv[])
{
    WindowMode mode = WindowMode::Window;

    if (argc > 1)
    {
        if (strcmp(argv[1], "fullscreen") == 0)
            mode = WindowMode::Fullscreen;
        else if (strcmp(argv[1], "borderless") == 0)
            mode = WindowMode::FullscreenWindowed;
    }

    std::cout << "VulkanTriangle - Starting in ";
    switch (mode)
    {
        case WindowMode::Fullscreen: std::cout << "FULLSCREEN"; break;
        case WindowMode::FullscreenWindowed: std::cout << "FULLSCREEN_WINDOWED"; break;
        default: std::cout << "WINDOW"; break;
    }
    std::cout << " mode" << std::endl;

    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return 1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* window = nullptr;

    switch (mode)
    {
        case WindowMode::Fullscreen:
        {
            GLFWmonitor* monitor = glfwGetPrimaryMonitor();
            const GLFWvidmode* vidmode = glfwGetVideoMode(monitor);
            window = glfwCreateWindow(vidmode->width, vidmode->height, "VulkanTriangle", monitor, nullptr);
            break;
        }
        case WindowMode::FullscreenWindowed:
        {
            GLFWmonitor* monitor = glfwGetPrimaryMonitor();
            const GLFWvidmode* vidmode = glfwGetVideoMode(monitor);
            glfwWindowHint(GLFW_RED_BITS, vidmode->redBits);
            glfwWindowHint(GLFW_GREEN_BITS, vidmode->greenBits);
            glfwWindowHint(GLFW_BLUE_BITS, vidmode->blueBits);
            glfwWindowHint(GLFW_REFRESH_RATE, vidmode->refreshRate);
            window = glfwCreateWindow(vidmode->width, vidmode->height, "VulkanTriangle", monitor, nullptr);
            break;
        }
        default:
            window = glfwCreateWindow(800, 600, "VulkanTriangle", nullptr, nullptr);
            break;
    }

    if (!window)
    {
        std::cerr << "Failed to create window" << std::endl;
        glfwTerminate();
        return 1;
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "VulkanTriangle";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = glfwExtensionCount;
    createInfo.ppEnabledExtensionNames = glfwExtensions;
    createInfo.enabledLayerCount = 0;

    VkInstance instance;
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
    {
        std::cerr << "Failed to create Vulkan instance" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    std::cout << "Vulkan instance created successfully" << std::endl;

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0)
    {
        std::cerr << "No Vulkan-capable devices found" << std::endl;
        vkDestroyInstance(instance, nullptr);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    std::cout << "Found " << deviceCount << " Vulkan device(s)" << std::endl;

    std::cout << "Press any key to exit..." << std::endl;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        for (int key = GLFW_KEY_SPACE; key <= GLFW_KEY_LAST; ++key)
        {
            if (glfwGetKey(window, key) != GLFW_RELEASE)
            {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
                break;
            }
        }
    }

    vkDestroyInstance(instance, nullptr);
    glfwDestroyWindow(window);
    glfwTerminate();

    std::cout << "VulkanTriangle - Exiting" << std::endl;
    return 0;
}