#pragma once

#include "Camera.h"
#include "Context.h"
#include "Image2D.h"
#include "GuiRenderer.h"
#include "Logger.h"
#include "MappedBuffer.h"
#include "Model.h"
#include "Pipeline.h"
#include "Sampler.h"
#include "StorageBuffer.h"
#include "Swapchain.h"
#include "Window.h"
#include "Renderer.h"
#include "ShaderManager.h"

#include <format>
#include <fstream>
#include <glm/gtx/string_cast.hpp>

namespace hlab {

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

class Application
{
  public:
    Application();
    ~Application();

    void run();
    void updateGui();
    void handleMouseMove(int32_t x, int32_t y);

  private:
    const uint32_t kMaxFramesInFlight = 2;
    const string kAssetsPathPrefix = "../../assets/";
    const string kShaderPathPrefix = kAssetsPathPrefix + "shaders/";

    Window window_;
    Context ctx_;
    Swapchain swapchain_;

    VkExtent2D windowSize_{};
    VkSampleCountFlagBits msaaSamples_ = VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM;

    MouseState mouseState_;
    Camera camera_;

    vector<CommandBuffer> commandBuffers_{};

    vector<VkFence> waitFences_{};

    vector<VkSemaphore> presentCompleteSemaphores_{};
    vector<VkSemaphore> renderCompleteSemaphores_{};

    ShaderManager shaderManager_;

    vector<Model> models_{};

    GuiRenderer guiRenderer_;
    Renderer renderer_;
};

} // namespace hlab