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
#include "engine/Image2D.h"
#include "engine/Sampler.h"

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
    float environmentIntensity = 1.0f; // Environment map intensity multiplier
    float roughnessLevel = 0.5f;       // Mip level for prefiltered map (0.0 = sharpest)
    uint32_t useIrradianceMap = 0;     // 0 = use prefiltered, 1 = use irradiance

    // Skybox visualization and debug
    uint32_t showMipLevels = 0; // Visualize mip levels as colors
    uint32_t showCubeFaces = 0; // Visualize cube faces as colors
    float padding1;
    float padding2;
    float padding3;
};

// Post-processing options uniform buffer structure
struct PostProcessingOptionsUBO
{
    // Tone mapping options
    int32_t toneMappingType = 2; // 0=None, 1=Reinhard, 2=ACES, 3=Uncharted2, 4=GT, 5=Lottes,
                                 // 6=Exponential, 7=ReinhardExtended, 8=Luminance, 9=Hable
    float exposure = 1.0f;       // HDR exposure adjustment
    float gamma = 2.2f;          // Gamma correction value
    float maxWhite = 11.2f;      // For extended Reinhard tone mapping

    // Color grading
    float contrast = 1.0f;   // Contrast adjustment
    float brightness = 0.0f; // Brightness adjustment
    float saturation = 1.0f; // Color saturation
    float vibrance = 0.0f;   // Vibrance (smart saturation)

    // Effects
    float vignetteStrength = 0.0f;    // Vignette effect strength
    float vignetteRadius = 0.8f;      // Vignette radius
    float filmGrainStrength = 0.0f;   // Film grain noise strength
    float chromaticAberration = 0.0f; // Chromatic aberration strength

    // Debug and visualization
    int32_t debugMode =
        0; // 0=Off, 1=Show tone mapping comparison, 2=Show color channels, 3=Split comparison
    float debugSplit = 0.5f;     // Split position for comparison (0.0-1.0)
    int32_t showOnlyChannel = 0; // 0=All, 1=Red, 2=Green, 3=Blue, 4=Alpha, 5=Luminance
    float padding1 = 0.0f;
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

class Ex11_PostProcessingExample
{
  public:
    Ex11_PostProcessingExample();
    ~Ex11_PostProcessingExample();

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

    // Rendering pipelines
    Pipeline skyPipeline_;
    // TODO: postPipeline_;

    // Render targets and textures
    SkyTextures skyTextures_;
    // TODO: Image2D hdrColorBuffer_; // HDR color buffer for post-processing input

    Sampler samplerLinearRepeat_;
    Sampler samplerLinearClamp_;

    // Uniform buffer objects
    SceneDataUBO sceneDataUBO_;
    SkyOptionsUBO skyOptionsUBO_;

    // Uniform buffers
    std::vector<UniformBuffer<SceneDataUBO>> sceneDataUniforms_;
    std::vector<UniformBuffer<SkyOptionsUBO>> skyOptionsUniforms_;

    // Descriptor sets
    std::vector<DescriptorSet> sceneDescriptorSets_;
    DescriptorSet skyDescriptorSet_;

    // Methods
    void initializeSkybox();
    void initializePostProcessing();
    void renderFrame();
    void updateGui(VkExtent2D windowSize);
    void renderHDRControlWindow();
    void renderPostProcessingControlWindow();
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
