#pragma once

#include "BindingInfo.h"

namespace hlab {

// sky pipeline: skybox.vert + skybox.frag
// Skybox rendering with IBL cubemaps.
// vert uses: set0 binding 0 (SceneDataUBO)
// frag uses: set0 binding 1 (SkyOptionsUBO), set1 binding 0-2 (cubemaps, brdfLut)
struct SkyPipeline
{
    static auto layoutBindings() -> std::vector<std::vector<VkDescriptorSetLayoutBinding>>
    {
        return {
            // Set 0
            {
                {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                 VK_SHADER_STAGE_VERTEX_BIT, nullptr},
                {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                 VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            },
            // Set 1
            {
                {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                 VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
                {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                 VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
                {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                 VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            },
        };
    }

    static auto bindingInfos() -> std::vector<std::vector<BindingInfo>>
    {
        constexpr auto VS = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
        constexpr auto FS = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        return {
            // Set 0
            {
                bi::buffer("sceneData", 0, 0, VS),
                bi::buffer("skyOptions", 0, 1, FS),
            },
            // Set 1
            {
                bi::sampledImage("prefilteredMap", 1, 0, FS),
                bi::sampledImage("irradianceMap", 1, 1, FS),
                bi::sampledImage("brdfLut", 1, 2, FS),
            },
        };
    }

    static auto pushConstantRange() -> VkPushConstantRange
    {
        return {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 128};
    }
};

} // namespace hlab
