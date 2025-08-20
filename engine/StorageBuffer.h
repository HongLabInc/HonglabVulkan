#pragma once

#include "Context.h"
#include "VulkanTools.h"
#include <vulkan/vulkan.h>

namespace hlab {

class StorageBuffer
{
  public:
    StorageBuffer(Context& ctx) : ctx_(ctx)
    {
    }

    ~StorageBuffer()
    {
        cleanup();
    }

    void create(VkDeviceSize size, VkBufferUsageFlags additionalUsage = 0);

    VkBuffer buffer() const
    {
        return buffer_;
    }

    VkDeviceSize size() const
    {
        return size_;
    }

    void* map();
    void unmap();

    void copyData(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

    VkDescriptorBufferInfo getDescriptorInfo() const;

    void cleanup();

  private:
    Context& ctx_;

    VkBuffer buffer_{VK_NULL_HANDLE};
    VkDeviceMemory memory_{VK_NULL_HANDLE};
    VkDeviceSize size_{0};
    void* mapped_{nullptr};
    bool hostVisible_{false};
};

} // namespace hlab