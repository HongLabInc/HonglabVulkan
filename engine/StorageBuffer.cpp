#include "StorageBuffer.h"
#include "VulkanTools.h"
#include "CommandBuffer.h"

namespace hlab {

StorageBuffer::StorageBuffer(Context& ctx) : ResourceBase(ctx, Type::Buffer)
{
}

StorageBuffer::~StorageBuffer()
{
    cleanup();
}

void StorageBuffer::create(VkDeviceSize size, VkBufferUsageFlags additionalUsage)
{
    const VkDevice device = ctx_.device();
    size_ = size;

    VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                               additionalUsage;

    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = size_;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    check(vkCreateBuffer(device, &bufferInfo, nullptr, &buffer_));

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer_, &memRequirements);
    uint32_t memoryTypeIndex = ctx_.getMemoryTypeIndex(memRequirements.memoryTypeBits,
                                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memoryTypeIndex == uint32_t(-1)) {
        memoryTypeIndex = ctx_.getMemoryTypeIndex(memRequirements.memoryTypeBits,
                                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        hostVisible_ = true;
    }

    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;
    check(vkAllocateMemory(device, &allocInfo, nullptr, &memory_));
    check(vkBindBufferMemory(device, buffer_, memory_, 0));
    
    // Initialize the resource
    initializeBufferResource(buffer_, size_);
}

void* StorageBuffer::map()
{
    if (!hostVisible_ || mapped_ != nullptr || buffer_ == VK_NULL_HANDLE) {
        return mapped_;
    }

    const VkDevice device = ctx_.device();
    check(vkMapMemory(device, memory_, 0, size_, 0, &mapped_));
    return mapped_;
}

void StorageBuffer::unmap()
{
    if (mapped_ != nullptr && buffer_ != VK_NULL_HANDLE) {
        const VkDevice device = ctx_.device();
        vkUnmapMemory(device, memory_);
        mapped_ = nullptr;
    }
}

void StorageBuffer::copyData(const void* data, VkDeviceSize size, VkDeviceSize offset)
{
    if (buffer_ == VK_NULL_HANDLE)
        return;

    if (hostVisible_) {
        void* mappedData = map();
        if (mappedData) {
            memcpy(static_cast<char*>(mappedData) + offset, data, size);
        }
    } else {
        // Use staging buffer for device-local memory
        const VkDevice device = ctx_.device();

        // Create staging buffer
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;

        VkBufferCreateInfo stagingBufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        stagingBufferInfo.size = size;
        stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        check(vkCreateBuffer(device, &stagingBufferInfo, nullptr, &stagingBuffer));

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, stagingBuffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = ctx_.getMemoryTypeIndex(
            memRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        check(vkAllocateMemory(device, &allocInfo, nullptr, &stagingMemory));
        check(vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0));

        // Copy data to staging buffer
        void* stagingMapped;
        check(vkMapMemory(device, stagingMemory, 0, size, 0, &stagingMapped));
        memcpy(stagingMapped, data, size);
        vkUnmapMemory(device, stagingMemory);

        // Copy from staging buffer to storage buffer
        CommandBuffer commandBuffer =
            ctx_.createTransferCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = offset;
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer.handle(), stagingBuffer, buffer_, 1, &copyRegion);

        commandBuffer.submitAndWait();

        // Cleanup staging resources
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingMemory, nullptr);
    }
}

VkDescriptorBufferInfo StorageBuffer::getDescriptorInfo() const
{
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = buffer_;
    bufferInfo.offset = 0;
    bufferInfo.range = size_;
    return bufferInfo;
}

void StorageBuffer::cleanup()
{
    const VkDevice device = ctx_.device();

    unmap();

    if (buffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, buffer_, nullptr);
        buffer_ = VK_NULL_HANDLE;
    }

    if (memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device, memory_, nullptr);
        memory_ = VK_NULL_HANDLE;
    }

    size_ = 0;
    hostVisible_ = false;
}

} // namespace hlab