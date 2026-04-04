#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

namespace hlab {

struct BindingInfo
{
    std::string resourceName{};             // name of binding resource
    uint32_t setIndex{0};                   // descriptor set index (0, 1, 2, ...)
    uint32_t bindingIndex{0};               // binding index within the set (0, 1, 2, ...)
    VkImageLayout targetLayout;             // required image layout for this binding
    VkAccessFlags2 targetAccess;            // required access flags for this binding
    VkPipelineStageFlags2 targetStage;      // pipeline stage where this binding is used
    bool writeonly{false};
};

// Helpers for constructing BindingInfo
namespace bi {

inline BindingInfo buffer(const std::string& name, uint32_t set, uint32_t binding,
                          VkPipelineStageFlags2 stage)
{
    return {name, set, binding, VK_IMAGE_LAYOUT_UNDEFINED, VK_ACCESS_2_SHADER_READ_BIT, stage,
            false};
}

inline BindingInfo sampledImage(const std::string& name, uint32_t set, uint32_t binding,
                                VkPipelineStageFlags2 stage)
{
    return {name, set, binding, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_2_SHADER_READ_BIT, stage, false};
}

inline BindingInfo storageImageRead(const std::string& name, uint32_t set, uint32_t binding,
                                    VkPipelineStageFlags2 stage)
{
    return {name, set, binding, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_2_SHADER_READ_BIT, stage,
            false};
}

inline BindingInfo storageImageReadWrite(const std::string& name, uint32_t set, uint32_t binding,
                                         VkPipelineStageFlags2 stage)
{
    return {name, set, binding, VK_IMAGE_LAYOUT_GENERAL,
            VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT, stage, false};
}

inline BindingInfo storageImageWrite(const std::string& name, uint32_t set, uint32_t binding,
                                     VkPipelineStageFlags2 stage)
{
    return {name, set, binding, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_2_SHADER_WRITE_BIT, stage,
            true};
}

} // namespace bi
} // namespace hlab
