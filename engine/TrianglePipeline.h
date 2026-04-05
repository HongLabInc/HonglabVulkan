#pragma once

#include "BindingInfo.h"

namespace hlab {

// triangle pipeline: triangle.vert + triangle.frag
// Simple hardcoded triangle — no descriptor sets, no push constants.
struct TrianglePipeline
{
    static auto layoutBindings() -> std::vector<std::vector<VkDescriptorSetLayoutBinding>>
    {
        return {}; // no descriptor sets
    }

    static auto bindingInfos() -> std::vector<std::vector<BindingInfo>>
    {
        return {}; // no bindings
    }

    static auto pushConstantRange() -> VkPushConstantRange
    {
        return {0, 0, 0}; // no push constants
    }
};

} // namespace hlab
