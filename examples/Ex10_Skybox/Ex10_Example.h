#pragma once

#include "engine/Context.h"
#include "engine/Window.h"
#include "engine/Swapchain.h"
#include "engine/CommandBuffer.h"
#include "engine/GuiRenderer.h"
#include "engine/ShaderManager.h"
#include "engine/Camera.h"
#include "engine/Pipeline.h"
#include "engine/SkyTextures.h"
#include "engine/UniformBuffer.h"
#include "engine/DescriptorSet.h"

#include <vector>
#include <glm/glm.hpp>

namespace hlab {

// Scene data UBO structure matching skybox.vert
struct SceneDataUBO
{
    glm::mat4 projection;
    glm::mat4 view;
    glm::vec3 cameraPos;
    float padding1;
    glm::vec3 directionalLightDir{-1.0f, -1.0f, -1.0f};
    float padding2;
    glm::vec3 directionalLightColor{1.0f, 1.0f, 1.0f};
    float padding3;
    glm::mat4 lightSpaceMatrix{1.0f};
};

// HDR skybox-specific control options
struct SkyOptionsUBO
{
    // HDR Environment mapping controls
    float exposure = 1.0f;             // HDR exposure adjustment
    float environmentIntensity = 1.0f; // Environment map intensity multiplier
    float roughnessLevel = 0.5f;       // Mip level for prefiltered map (0.0 = sharpest)
    uint32_t useIrradianceMap = 0;     // 0 = use prefiltered, 1 = use irradiance

    // Skybox visualization and debug
    uint32_t showMipLevels = 0; // Visualize mip levels as colors
    uint32_t showCubeFaces = 0; // Visualize cube faces as colors
    float padding1;
    float padding2;
};

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

    void mainLoop();

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
    MouseState mouseState_;
    uint32_t currentFrame_{0};
    uint32_t currentSemaphore_{0};
    bool shouldClose_{false};

    // Camera
    Camera camera_;

    // Skybox rendering
    std::unique_ptr<Pipeline> skyPipeline_;
    SkyTextures skyTextures_;
    SceneDataUBO sceneDataUBO_;
    SkyOptionsUBO skyOptionsUBO_;
    std::vector<UniformBuffer<SceneDataUBO>> sceneDataUniforms_;
    std::vector<UniformBuffer<SkyOptionsUBO>> skyOptionsUniforms_;
    std::vector<DescriptorSet> sceneDescriptorSets_;
    DescriptorSet skyDescriptorSet_;

    // Methods
    void initializeSkybox();
    void renderFrame();
    void updateGui(VkExtent2D windowSize);
    void renderHDRControlWindow();
    void recordCommandBuffer(CommandBuffer& cmd, uint32_t imageIndex, VkExtent2D windowSize);
    void submitFrame(CommandBuffer& commandBuffer, VkSemaphore waitSemaphore,
                     VkSemaphore signalSemaphore, VkFence fence);

    // Mouse control for camera
    void handleMouseMove(int32_t x, int32_t y);

    // Static callbacks (will delegate to instance methods)
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

    // Instance callback handlers
    void handleKeyInput(int key, int scancode, int action, int mods);
    void handleMouseButton(int button, int action, int mods);
    void handleCursorPos(double xpos, double ypos);
    void handleScroll(double xoffset, double yoffset);
};

} // namespace hlab
