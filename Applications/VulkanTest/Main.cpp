// Created by mooming.go@gmail.com

#include "Engine/Engine.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>


namespace hbe
{
enum class EDisplayMode : uint8_t
{
	Window,
	FullScreen,
	WindowedFullScreen
};

class WindowApplication
{
private:
	EDisplayMode displayMode;
	bool isResizable;
	int width;
	int height;
	StaticString title;
	GLFWwindow* window;
	VkInstance vkInstance;

public:
	WindowApplication(const char* title)
		: displayMode(EDisplayMode::FullScreen)
		, isResizable(true)
		, width(-1)
		, height(-1)
		, title(title)
		, window(nullptr)
		, vkInstance(nullptr)
	{}

	void SetDisplayMode(EDisplayMode mode)
	{
		displayMode = mode;
	}

	void SetResolution(int inWidth, int inHeight)
	{
		width = inWidth;
		height = inHeight;
	}

	void Initialize()
	{
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		auto monitor = glfwGetPrimaryMonitor();
		const GLFWvidmode* mode = glfwGetVideoMode(monitor);
		glfwWindowHint(GLFW_RED_BITS, mode->redBits);
		glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
		glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
		glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);

		if (width < 0)
		{
			width = mode->width;
		}

		if (height < 0)
		{
			height = mode->height;
		}

		window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);

		switch (displayMode)
		{
			case EDisplayMode::Window:
				glfwSetWindowMonitor(window, nullptr, 0, 0, width, height, mode->refreshRate);
				break;
			case EDisplayMode::FullScreen:
				glfwSetWindowMonitor(window, monitor, 0, 0, width, height, mode->refreshRate);
				break;
			case EDisplayMode::WindowedFullScreen:
				glfwSetWindowMonitor(window, monitor, 0, 0, width, height, mode->refreshRate);
				break;
		}

		glfwSetWindowAttrib(window, GLFW_RESIZABLE, isResizable ? GLFW_TRUE : GLFW_FALSE);

		CreateVKInstance();
	}

	void Update()
	{
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();
		}
	}

	void Cleanup()
	{
		vkDestroyInstance(vkInstance, nullptr);
		glfwDestroyWindow(window);
		glfwTerminate();
	}

private:
	bool CreateVKInstance()
	{
		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = title.c_str();
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		createInfo.enabledExtensionCount = glfwExtensionCount;
		createInfo.ppEnabledExtensionNames = glfwExtensions;
		createInfo.enabledLayerCount = 0;

		const VkResult result = vkCreateInstance(&createInfo, nullptr, &vkInstance);
		if (result != VK_SUCCESS)
			{
			return false;
		}

		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> extensions(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

		std::cout << "available extensions:\n";

		for (const auto& extension : extensions) {
			std::cout << '\t' << extension.extensionName << '\n';
		}

		return true;
	}
};
} // namespace hbbe

int main(int argc, const char* argv[])
{
	using namespace hbe;

    Engine hengine;
    hengine.Initialize(argc, argv);

	WindowApplication app("Vulkan Example");

	app.SetDisplayMode(EDisplayMode::Window);
	app.Initialize();
	app.Update();
	app.Cleanup();

    hengine.ShutDown();
    hengine.WaitForEnd();

    return 0;
}
