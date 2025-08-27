#pragma once

#include "engine/Context.h"
#include "engine/Window.h"
#include "engine/Swapchain.h"
#include "engine/CommandBuffer.h"
#include "engine/GuiRenderer.h"
#include "engine/ShaderManager.h"

#include <vector>
#include <glm/glm.hpp>

namespace hlab {

// Mouse state structure
struct MouseState
{
    struct
    {
        bool left = false;
        bool right = false;
        bool middle = false;
    } buttons;
    glm::vec2 position{0.0f, 0.0f};
};

class Ex10_Example
{
  public:
    Ex10_Example();
    ~Ex10_Example();

    void run();

  private:
    const uint32_t kMaxFramesInFlight = 2;
    const string kAssetsPathPrefix = "../../assets/";
    const string kShaderPathPrefix = kAssetsPathPrefix + "shaders/";

    // Core Vulkan objects
    Window window_;
    Context ctx_;
    VkExtent2D windowSize_;
    Swapchain swapchain_;
    ShaderManager shaderManager_;
    GuiRenderer guiRenderer_;

    // Frame resources
    std::vector<CommandBuffer> commandBuffers_;
    std::vector<VkSemaphore> presentSemaphores_;
    std::vector<VkSemaphore> renderSemaphores_;
    std::vector<VkFence> inFlightFences_;

    // Application state
    glm::vec4 clearColor_{0.2f, 0.3f, 0.5f, 1.0f};
    MouseState mouseState_;
    uint32_t currentFrame_{0};
    uint32_t currentSemaphore_{0};
    bool shouldClose_{false};

    // Methods
    void mainLoop();
    void renderFrame();
    void updateGui(VkExtent2D windowSize);
    void renderColorControlWindow();
    void renderColorPresets();
    void recordCommandBuffer(CommandBuffer& cmd, uint32_t imageIndex, VkExtent2D windowSize);
    void submitFrame(CommandBuffer& commandBuffer, VkSemaphore waitSemaphore,
                     VkSemaphore signalSemaphore, VkFence fence);

    // Static callbacks (will delegate to instance methods)
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);

    // Instance callback handlers
    void handleKeyInput(int key, int scancode, int action, int mods);
    void handleMouseButton(int button, int action, int mods);
    void handleCursorPos(double xpos, double ypos);
};

} // namespace hlab
