#pragma once

#include "BindingInfo.h"

namespace hlab {

// compute pipeline: test.comp
// set 0, binding 0: inputImage  (storage image, readonly)
// set 0, binding 1: outputImage (storage image, writeonly)
struct ComputePipeline
{
    static auto layoutBindings() -> std::vector<std::vector<VkDescriptorSetLayoutBinding>>
    {
        return {{
            {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
             VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
             VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        }};
    }

    static auto bindingInfos() -> std::vector<std::vector<BindingInfo>>
    {
        constexpr auto CS = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        return {{
            bi::storageImageRead("inputImage", 0, 0, CS),
            bi::storageImageWrite("outputImage", 0, 1, CS),
        }};
    }

    static constexpr std::array<uint32_t, 3> kLocalSize = {16, 16, 1};
};

} // namespace hlab
