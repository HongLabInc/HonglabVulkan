#pragma once

#include "Context.h"
#include "MappedBuffer.h"
#include "Pipeline.h"
#include "Image2D.h"
#include "Sampler.h"
#include "PushConstants.h"
#include "DescriptorSet.h"
#include <glm/glm.hpp>
#include <imgui.h>

namespace hlab {

using namespace std;
using namespace glm;

struct PushConstBlock
{
    glm::vec2 scale{1.0f, 1.0f};
    glm::vec2 translate{0.0f, 0.0f};
};

class GuiRenderer
{
  public:
    GuiRenderer(Context& ctx, ShaderManager& shaderManager, VkFormat colorFormat);
    ~GuiRenderer();

    void draw(const VkCommandBuffer cmd, VkImageView swapchainImageView, VkViewport viewport);
    void resize(uint32_t width, uint32_t height);

    auto update() -> bool;
    auto imguiPipeline() -> Pipeline&;

  private:
    Context& ctx_;
    ShaderManager& shaderManager_;

    MappedBuffer vertexBuffer_;
    MappedBuffer indexBuffer_;
    uint32_t vertexCount_{0};
    uint32_t indexCount_{0};

    Image2D fontImage_;
    Sampler fontSampler_;
    Pipeline guiPipeline_;

    DescriptorSet fontSet_;
    PushConstants<PushConstBlock> pushConsts_;

    bool visible_{true};
    bool updated_{false};
    float scale_{1.4f};
    float updateTimer_{0.0f};
};

} // namespace hlab