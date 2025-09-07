#pragma once

#include "Resource.h"
#include <vulkan/vulkan.h>

namespace hlab {

class StorageBuffer : public Resource
{
  public:
    StorageBuffer(Context& ctx);
    ~StorageBuffer();

    // Accessors
    VkBuffer buffer() const
    {
        return buffer_;
    }
    VkDeviceSize size() const
    {
        return size_;
    }
    VkDescriptorBufferInfo getDescriptorInfo() const;

    // Buffer operations
    void create(VkDeviceSize size, VkBufferUsageFlags additionalUsage = 0);
    void* map();
    void unmap();
    void copyData(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    void cleanup() override;

    void updateBinding(VkDescriptorSetLayoutBinding& binding) override
    {
        // Set up binding information for storage buffer
        binding.binding = 0; // This will be overridden by DescriptorSet::create()
        binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        binding.descriptorCount = 1;
        binding.pImmutableSamplers = nullptr;
        binding.stageFlags = 0; // Will be filled by shader reflection
    }

    void updateWrite(VkWriteDescriptorSet& write) override
    {
        // Set up the buffer info for the descriptor write
        static VkDescriptorBufferInfo bufferInfo;
        bufferInfo.buffer = buffer_;
        bufferInfo.offset = 0;
        bufferInfo.range = size_;

        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.pNext = nullptr;
        write.dstSet = VK_NULL_HANDLE; // Will be set by DescriptorSet::create()
        write.dstBinding = 0;          // Will be set by DescriptorSet::create()
        write.dstArrayElement = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.pImageInfo = nullptr;
        write.pBufferInfo = &bufferInfo;
        write.pTexelBufferView = nullptr;
    }

  private:
    VkBuffer buffer_{VK_NULL_HANDLE};
    VkDeviceMemory memory_{VK_NULL_HANDLE};
    VkDeviceSize size_{0};
    void* mapped_{nullptr};
    bool hostVisible_{false};
};

} // namespace hlab