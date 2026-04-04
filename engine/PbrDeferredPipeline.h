#pragma once

#include "BindingInfo.h"

namespace hlab {

// pbrDeferred pipeline: pbrForward.vert + pbrDeferred.frag
// PBR geometry pass writing to G-buffer.
// vert uses: set0 binding 0-2 (SceneDataUBO, OptionsUBO, BoneDataUBO)
// frag uses: set0 binding 0-1 (SceneDataUBO, OptionsUBO), set1 binding 0-1 (MaterialBuffer, materialTextures)
struct PbrDeferredPipeline
{
    static auto layoutBindings() -> std::vector<std::vector<VkDescriptorSetLayoutBinding>>
    {
        return {
            // Set 0
            {
                {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                 VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
                {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                 VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
                {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                 VK_SHADER_STAGE_VERTEX_BIT, nullptr},
            },
            // Set 1
            {
                {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                 VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
                {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 512,
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
                bi::buffer("options", 0, 1, VS),
                bi::buffer("boneData", 0, 2, VS),
            },
            // Set 1
            {
                bi::buffer("materialBuffer", 1, 0, FS),
                bi::sampledImage("materialTextures", 1, 1, FS),
            },
        };
    }

    static auto pushConstantRange() -> VkPushConstantRange
    {
        return {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 128};
    }
};

} // namespace hlab
