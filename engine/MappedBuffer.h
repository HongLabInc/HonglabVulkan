#pragma once

#include "Context.h"
#include "ResourceBase.h"

#include <string>
#include <vulkan/vulkan.h>

namespace hlab {

class MappedBuffer : public ResourceBase
{
  public:
    MappedBuffer(Context& ctx);
    MappedBuffer(MappedBuffer&&) noexcept;
    MappedBuffer(const MappedBuffer&) = delete;
    MappedBuffer& operator=(const MappedBuffer&) = delete;
    MappedBuffer& operator=(MappedBuffer&&) = delete;
    ~MappedBuffer();

    auto buffer() -> VkBuffer&;
    auto descriptorBufferInfo() const -> VkDescriptorBufferInfo;
    auto mapped() const -> void*;
    auto name() -> string&;
    
    // Legacy interface for backward compatibility
    auto resourceBinding() -> ResourceBinding&
    {
        return ResourceBase::resourceBinding();
    }
    
    // Add getters for size information
    auto allocatedSize() const -> VkDeviceSize { return allocatedSize_; }
    auto dataSize() const -> VkDeviceSize { return dataSize_; }

    void cleanup() override;
    void create(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memPropFlags,
                VkDeviceSize size, void* data);
    void createVertexBuffer(VkDeviceSize size, void* data);
    void createIndexBuffer(VkDeviceSize size, void* data);
    void createStagingBuffer(VkDeviceSize size, void* data);
    void createUniformBuffer(VkDeviceSize size, void* data);
    void updateData(const void* data, VkDeviceSize size, VkDeviceSize offset);
    void flush() const;

  private:
    VkBuffer buffer_{VK_NULL_HANDLE};
    VkDeviceMemory memory_{VK_NULL_HANDLE};

    VkDeviceSize offset_{0};
    VkDeviceSize dataSize_{0};
    VkDeviceSize allocatedSize_{0};
    VkDeviceSize alignment_{0};

    VkMemoryPropertyFlags memPropFlags_{VK_MEMORY_PROPERTY_FLAG_BITS_MAX_ENUM};
    VkBufferUsageFlags usageFlags_{VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM};

    void* mapped_{nullptr};

    string name_{};
};

} // namespace hlab