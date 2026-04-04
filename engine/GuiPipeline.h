#pragma once

#include "BindingInfo.h"

namespace hlab {

// gui pipeline: imgui.vert + imgui.frag
// ImGui rendering.
// vert uses: push constants only (no descriptors)
// frag uses: set0 binding 0 (fontSampler)
struct GuiPipeline
{
    static auto layoutBindings() -> std::vector<std::vector<VkDescriptorSetLayoutBinding>>
    {
        return {{
            {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
             VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        }};
    }

    static auto bindingInfos() -> std::vector<std::vector<BindingInfo>>
    {
        constexpr auto FS = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        return {{
            bi::sampledImage("fontSampler", 0, 0, FS),
        }};
    }

    static auto pushConstantRange() -> VkPushConstantRange
    {
        return {VK_SHADER_STAGE_VERTEX_BIT, 0, 16}; // vec2 scale + vec2 translate
    }
};

} // namespace hlab
