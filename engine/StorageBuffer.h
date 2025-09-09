#pragma once

#include "ResourceBase.h"
#include <vulkan/vulkan.h>

namespace hlab {

class StorageBuffer : public ResourceBase
{
  public:
    StorageBuffer(Context& ctx);
    ~StorageBuffer();

    // Accessors
    VkBuffer buffer() const { return buffer_; }
    VkDeviceSize size() const { return size_; }
    VkDescriptorBufferInfo getDescriptorInfo() const;

    // Buffer operations
    void create(VkDeviceSize size, VkBufferUsageFlags additionalUsage = 0);
    void* map();
    void unmap();
    void copyData(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    void cleanup() override;

  private:
    VkBuffer buffer_{VK_NULL_HANDLE};
    VkDeviceMemory memory_{VK_NULL_HANDLE};
    VkDeviceSize size_{0};
    void* mapped_{nullptr};
    bool hostVisible_{false};
};

} // namespace hlab