#pragma once

#include "BindingInfo.h"
#include <array>

namespace hlab {

// deferredLighting pipeline: deferredLighting.comp
// Compute shader for deferred PBR lighting with SSAO.
// All bindings are COMPUTE stage only.
struct DeferredLightingPipeline
{
    static auto layoutBindings() -> std::vector<std::vector<VkDescriptorSetLayoutBinding>>
    {
        return {{
            {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {9, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {10, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {11, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {12, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {13, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        }};
    }

    static auto bindingInfos() -> std::vector<std::vector<BindingInfo>>
    {
        constexpr auto CS = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        return {{
            bi::buffer("sceneData", 0, 0, CS),
            bi::buffer("options", 0, 1, CS),
            bi::buffer("ssaoOptions", 0, 2, CS),
            bi::storageImageRead("floatColor1", 0, 3, CS),
            bi::storageImageWrite("floatColor2", 0, 4, CS),
            bi::sampledImage("depthStencil", 0, 5, CS),
            bi::sampledImage("gAlbedo", 0, 6, CS),
            bi::sampledImage("gNormal", 0, 7, CS),
            bi::sampledImage("gPosition", 0, 8, CS),
            bi::sampledImage("gMaterial", 0, 9, CS),
            bi::sampledImage("shadowMap", 0, 10, CS),
            bi::sampledImage("prefilteredMap", 0, 11, CS),
            bi::sampledImage("irradianceMap", 0, 12, CS),
            bi::sampledImage("brdfLut", 0, 13, CS),
        }};
    }

    static auto pushConstantRange() -> VkPushConstantRange
    {
        return {0, 0, 0}; // No push constants
    }

    static constexpr std::array<uint32_t, 3> kLocalSize = {16, 16, 1};
};

} // namespace hlab
