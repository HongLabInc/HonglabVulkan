#pragma once

#include "BindingInfo.h"

namespace hlab {

// post pipeline: post.vert + post.frag
// Fullscreen post-processing (tone mapping, color grading, effects).
// vert uses: no descriptors (fullscreen quad via gl_VertexIndex)
// frag uses: set0 binding 0-2 (floatColor2, PostProcessingOptions, depthStencil)
struct PostPipeline
{
    static auto layoutBindings() -> std::vector<std::vector<VkDescriptorSetLayoutBinding>>
    {
        return {{
            {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
             VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
             VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
             VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        }};
    }

    static auto bindingInfos() -> std::vector<std::vector<BindingInfo>>
    {
        constexpr auto FS = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        return {{
            bi::sampledImage("floatColor2", 0, 0, FS),
            bi::buffer("postOptions", 0, 1, FS),
            bi::sampledImage("depthStencil", 0, 2, FS),
        }};
    }

    static auto pushConstantRange() -> VkPushConstantRange
    {
        return {0, 0, 0}; // No push constants
    }
};

} // namespace hlab
