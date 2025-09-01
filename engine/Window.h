#pragma once

// 주의: 윈도우즈 운영체제에서 제공하는 "Windows.h"와 다릅니다.

#include "Logger.h"
#include <GLFW/glfw3.h>
#include <vector>
#include <vulkan/vulkan.h>

namespace hlab {

using namespace std;

class Window
{
  public:
    Window();
    ~Window();

    auto createSurface(VkInstance instance) -> VkSurfaceKHR;
    auto getFramebufferSize() const -> VkExtent2D;
    auto getRequiredExtensions() -> vector<const char*>;

    bool isCloseRequested() const;
    bool isMinimized() const;

    void pollEvents();
    void setUserPointer(void* pointer); // 아래 콜백 함수들을 등록하기 위해 필요
    void setKeyCallback(GLFWkeyfun callback);
    void setMouseButtonCallback(GLFWmousebuttonfun callback);
    void setCursorPosCallback(GLFWcursorposfun callback);
    void setScrollCallback(GLFWscrollfun callback);
    void setFramebufferSizeCallback(GLFWframebuffersizefun callback); // Add this line
    
    // Getter for GLFW window (needed for scaling calculations)
    GLFWwindow* getGLFWwindow() const { return glfwWindow_; }

    static GLFWwindow* createWindow();

  private:
    GLFWwindow* glfwWindow_ = nullptr;
};

} // namespace hlab
