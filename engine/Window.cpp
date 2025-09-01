#include "Window.h"
#include "VulkanTools.h"

#include <algorithm>
#include <GLFW/glfw3.h>

namespace hlab {

Window::Window()
{
    glfwWindow_ = createWindow();

    glfwSetErrorCallback([](int error, const char* description) {
        printf("GLFW Error (%i): %s\n", error, description);
        exitWithMessage("GLFW Error");
    });

    // 모니터(디스플레이) 중앙으로 윈도우 위치 이동
    const GLFWvidmode* videoMode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    int displayWidth = videoMode->width;
    int displayHeight = videoMode->height;

    int windowWidth, windowHeight;
    glfwGetWindowSize(glfwWindow_, &windowWidth, &windowHeight);

    // Center the window on the primary monitor
    int windowPosX = (displayWidth - windowWidth) / 2;
    int windowPosY = (displayHeight - windowHeight) / 2;
    glfwSetWindowPos(glfwWindow_, windowPosX, windowPosY);
}

Window::~Window()
{
    if (glfwWindow_) {
        glfwDestroyWindow(glfwWindow_);
        glfwTerminate();
    }
}

void Window::pollEvents()
{
    glfwPollEvents();

    // 키보드 이벤트를 실제로 처리하는 것은 glfwSetKeyCallback()에서 등록한 콜백함수
}

bool Window::isCloseRequested() const
{
    return glfwWindowShouldClose(glfwWindow_);
}

bool Window::isMinimized() const
{
    int width, height;
    glfwGetWindowSize(glfwWindow_, &width, &height);
    return width == 0 || height == 0;
}

GLFWwindow* Window::createWindow()
{
    constexpr float aspectRatio = 16.0f / 9.0f; // Use floating-point division
    constexpr float outRatio = 0.8f;            // 모니터 해상도의 80%로 윈도우 생성

    if (!glfwInit()) {
        exitWithMessage("GLFW not initialized");
    }

    const GLFWvidmode* videoMode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    int displayWidth = videoMode->width;
    int displayHeight = videoMode->height;
    // std::print("Current display: {} x {} at {}Hz\n", width, height, videoMode->refreshRate);

    int windowWidth, windowHeight;
    if (displayWidth > displayHeight) { // Landscape: base on height
        windowHeight = static_cast<int>(displayHeight * outRatio);
        windowWidth = static_cast<int>(windowHeight * aspectRatio);
    } else { // Portrait or square: base on width
        windowWidth = static_cast<int>(displayWidth * outRatio);
        windowHeight = static_cast<int>(windowWidth / aspectRatio);
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE); // MacOS scale fix

    GLFWwindow* window =
        glfwCreateWindow(windowWidth, windowHeight, "Honglab Vulkan", nullptr, nullptr);

    if (!window) {
        glfwTerminate();
        exitWithMessage("Failed to create GLFW window");
    }

    return window;
}

[[nodiscard]]
VkExtent2D Window::getFramebufferSize() const
{
    int width, height;
    glfwGetFramebufferSize(glfwWindow_, &width, &height);
    return VkExtent2D{static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
}

[[nodiscard]]
vector<const char*> Window::getRequiredExtensions()
{
    // OS에 따라 어떤 EXT가 필요한지 GLFW를 통해서 가져옵니다.

    vector<const char*> instanceExtensions{};

    uint32_t glfwExtensionCount = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    for (uint32_t i = 0; i < glfwExtensionCount; i++) {
        if (std::find(instanceExtensions.begin(), instanceExtensions.end(), extensions[i]) ==
            instanceExtensions.end()) {
            instanceExtensions.push_back(extensions[i]);
        }
    }

    instanceExtensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);

    printLog("GlfwRequiredInstanceExtensions: {}", glfwExtensionCount);
    for (uint32_t i = 0; i < glfwExtensionCount; ++i) {
        printLog("  {}", extensions[i]);
    }

    return instanceExtensions;
}

VkSurfaceKHR Window::createSurface(VkInstance instance)
{
    VkSurfaceKHR surface;

    auto res = glfwCreateWindowSurface(instance, glfwWindow_, nullptr, &surface);
    check(res);

    return surface;
}

void Window::setUserPointer(void* pointer)
{
    glfwSetWindowUserPointer(glfwWindow_, pointer);
}

void Window::setKeyCallback(GLFWkeyfun callback)
{
    glfwSetKeyCallback(glfwWindow_, callback);
}

void Window::setMouseButtonCallback(GLFWmousebuttonfun callback)
{
    glfwSetMouseButtonCallback(glfwWindow_, callback);
}

void Window::setCursorPosCallback(GLFWcursorposfun callback)
{
    glfwSetCursorPosCallback(glfwWindow_, callback);
}

void Window::setScrollCallback(GLFWscrollfun callback)
{
    glfwSetScrollCallback(glfwWindow_, callback);
}

void Window::setFramebufferSizeCallback(GLFWframebuffersizefun callback)
{
    glfwSetFramebufferSizeCallback(glfwWindow_, callback);
}

} // namespace hlab