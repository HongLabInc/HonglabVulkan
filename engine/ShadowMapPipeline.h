#pragma once

#include "BindingInfo.h"

namespace hlab {

// shadowMap pipeline: shadowMap.vert + shadowMap.frag
// Depth-only pass for shadow map generation.
// vert uses: set0 binding 0-2 (SceneDataUBO, OptionsUBO, BoneDataUBO)
// frag uses: push constants only (no descriptors)
struct ShadowMapPipeline
{
    static auto layoutBindings() -> std::vector<std::vector<VkDescriptorSetLayoutBinding>>
    {
        return {{
            {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
            {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        }};
    }

    static auto bindingInfos() -> std::vector<std::vector<BindingInfo>>
    {
        constexpr auto VS = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
        return {{
            bi::buffer("sceneData", 0, 0, VS),
            bi::buffer("options", 0, 1, VS),
            bi::buffer("boneData", 0, 2, VS),
        }};
    }

    static auto pushConstantRange() -> VkPushConstantRange
    {
        return {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 128};
    }
};

} // namespace hlab
