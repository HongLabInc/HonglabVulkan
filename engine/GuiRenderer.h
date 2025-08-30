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
#include <array>

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

    void draw(const VkCommandBuffer cmd, VkImageView swapchainImageView, VkViewport viewport, uint32_t frameIndex);
    void resize(uint32_t width, uint32_t height);

    auto update(uint32_t frameIndex) -> bool;
    auto imguiPipeline() -> Pipeline&;

  private:
    static constexpr uint32_t kMaxFramesInFlight = 2;
    
    struct FrameData {
        MappedBuffer vertexBuffer;
        MappedBuffer indexBuffer;
        
        FrameData(Context& ctx) : vertexBuffer(ctx), indexBuffer(ctx) {}
    };

    Context& ctx_;
    ShaderManager& shaderManager_;

    std::array<FrameData, kMaxFramesInFlight> frameData_;

    // Current frame data counters (updated each frame)
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