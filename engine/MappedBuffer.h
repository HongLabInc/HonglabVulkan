#pragma once

#include "Context.h"
#include "Resource.h"

#include <string>
#include <vulkan/vulkan.h>

namespace hlab {

class MappedBuffer : public Resource
{
  public:
    MappedBuffer(Context& ctx);
    MappedBuffer(MappedBuffer&&) = delete;
    MappedBuffer(const MappedBuffer&) = delete;
    MappedBuffer& operator=(const MappedBuffer&) = delete;
    MappedBuffer& operator=(MappedBuffer&&) = delete;
    ~MappedBuffer();

    auto buffer() -> VkBuffer&;
    auto descriptorBufferInfo() const -> VkDescriptorBufferInfo;
    auto mapped() const -> void*;
    auto name() -> string&;

    auto allocatedSize() const -> VkDeviceSize;

    void cleanup() override;

    // Implement required Resource methods
    void updateBinding(VkDescriptorSetLayoutBinding& binding) override;
    void updateWrite(VkWriteDescriptorSet& write) override;

    void create(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memPropFlags,
                VkDeviceSize size, void* data);
    void createVertexBuffer(VkDeviceSize size, void* data);
    void createIndexBuffer(VkDeviceSize size, void* data);
    void createStagingBuffer(VkDeviceSize size, void* data);
    void createUniformBuffer(VkDeviceSize size, void* data);
    void updateData(const void* data, VkDeviceSize size, VkDeviceSize offset);
    void flush() const;

    // Template methods to replace UniformBuffer functionality
    template<typename T>
    void createUniformBuffer(T& cpuData) {
        static_assert(std::is_trivially_copyable_v<T>,
                      "Uniform buffer data type must be trivially copyable");
        
        cpuData_ = &cpuData;
        cpuDataSize_ = sizeof(T);
        createUniformBuffer(sizeof(T), &cpuData);
    }

    // Update GPU buffer from CPU data
    void updateFromCpuData() {
        if (cpuData_ && cpuDataSize_ > 0) {
            updateData(cpuData_, cpuDataSize_, 0);
        }
    }

    // Get CPU data pointer (type-unsafe, for compatibility)
    void* getCpuData() const { return cpuData_; }

    // Type-safe CPU data access
    template<typename T>
    T& getCpuData() {
        return *static_cast<T*>(cpuData_);
    }

  private:
    VkBuffer buffer_{VK_NULL_HANDLE};
    VkDeviceMemory memory_{VK_NULL_HANDLE};

    VkDeviceSize offset_{0};
    VkDeviceSize dataSize_{0};
    VkDeviceSize allocatedSize_{0};
    VkDeviceSize alignment_{0};

    VkMemoryPropertyFlags memPropFlags_{VK_MEMORY_PROPERTY_FLAG_BITS_MAX_ENUM};
    VkBufferUsageFlags usageFlags_{VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM};

    string name_{};
    void* mapped_{nullptr};
    void* cpuData_{nullptr};      // Points to CPU-side data structure
    size_t cpuDataSize_{0};       // Size of CPU data for validation
};

} // namespace hlab