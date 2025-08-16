#pragma once

#include <cstring>
#include <vulkan/vulkan.h>

namespace hlab {

class Context; // Forward declaration

template <typename T_DATA>
class PushConstants
{
  public:
    PushConstants(Context &ctx) : ctx_(ctx)
    {
    }

    T_DATA &data()
    {
        return data_;
    }

    void push(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout)
    {
        vkCmdPushConstants(commandBuffer, pipelineLayout, stageFlags_, 0, sizeof(T_DATA), &data_);
    }

    size_t size() const
    {
        return sizeof(T_DATA);
    }

    VkPushConstantRange getPushConstantRange()
    {
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = stageFlags_;
        pushConstantRange.offset = 0;
        pushConstantRange.size = uint32_t(size());
        return pushConstantRange;
    }

    void setStageFlags(VkShaderStageFlags stageFlags)
    {
        stageFlags_ = stageFlags;
    }

  private:
    Context &ctx_;

    T_DATA data_{};
    VkShaderStageFlags stageFlags_{VK_SHADER_STAGE_ALL};
};

} // namespace hlab