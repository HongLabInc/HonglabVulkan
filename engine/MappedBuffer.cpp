#include "MappedBuffer.h"
#include "VulkanTools.h"

namespace hlab {

MappedBuffer::MappedBuffer(Context& ctx) : Resource(ctx, Type::Buffer)
{
}

void MappedBuffer::flush() const
{
    VkMappedMemoryRange mappedRange = {VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
    mappedRange.memory = memory_;
    mappedRange.offset = offset_;
    mappedRange.size = allocatedSize_;

    check(vkFlushMappedMemoryRanges(ctx_.device(), 1, &mappedRange));
}

MappedBuffer::~MappedBuffer()
{
    cleanup();
}

VkBuffer& MappedBuffer::buffer()
{
    return buffer_;
}

void* MappedBuffer::mapped() const
{
    return mapped_;
}

auto MappedBuffer::descriptorBufferInfo() const -> VkDescriptorBufferInfo
{
    VkDescriptorBufferInfo descriptor{};
    descriptor.buffer = buffer_;
    descriptor.offset = offset_;
    descriptor.range = allocatedSize_;

    return descriptor;
}

auto MappedBuffer::name() -> string&
{
    return name_;
}

void MappedBuffer::cleanup()
{
    if (mapped_) {
        vkUnmapMemory(ctx_.device(), memory_);
        mapped_ = nullptr;
    }
    if (buffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(ctx_.device(), buffer_, nullptr);
        buffer_ = VK_NULL_HANDLE;
    }
    if (memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(ctx_.device(), memory_, nullptr);
        memory_ = VK_NULL_HANDLE;
    }
}

void MappedBuffer::create(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memPropFlags,
                          VkDeviceSize dataSize, void* data)
{
    cleanup();

    usageFlags_ = usageFlags;
    memPropFlags_ = memPropFlags;
    dataSize_ = dataSize;
    offset_ = 0;

    VkBufferCreateInfo bufCreateInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufCreateInfo.usage = usageFlags_;
    bufCreateInfo.size = dataSize_;
    check(vkCreateBuffer(ctx_.device(), &bufCreateInfo, nullptr, &buffer_));

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(ctx_.device(), buffer_, &memReqs);

    allocatedSize_ = memReqs.size; // 실제로 할당된 크기, nonCoherentAtomSize(64)의 배수
    alignment_ = memReqs.alignment;

    // cout << "Requested size: " << dataSize_ << ", real allocated size: " << memReqs.size << endl;

    VkMemoryAllocateInfo memAlloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = ctx_.getMemoryTypeIndex(memReqs.memoryTypeBits, memPropFlags);

    VkMemoryAllocateFlagsInfoKHR allocFlagsInfo{};
    if (usageFlags_ & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
        allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR;
        allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
        memAlloc.pNext = &allocFlagsInfo;
    }
    check(vkAllocateMemory(ctx_.device(), &memAlloc, nullptr, &memory_));
    check(vkMapMemory(ctx_.device(), memory_, offset_, allocatedSize_, 0, &mapped_));

    if (data != nullptr) {
        memcpy(mapped_, data, dataSize_);
        if ((memPropFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
            flush();
    }

    check(vkBindBufferMemory(ctx_.device(), buffer_, memory_, offset_));
}

// Vertex/Index: Non-coherent (manual flush needed)
void MappedBuffer::createVertexBuffer(VkDeviceSize size, void* data)
{
    create(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, size, data);

    /* 메모
     * VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT와 같이
     * HOST_COHERENT를 추가할 경우 수동 flush() 호출이 불필요
     */
}

// Vertex/Index: Non-coherent (manual flush needed)
void MappedBuffer::createIndexBuffer(VkDeviceSize size, void* data)
{
    create(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, size, data);
}

// Staging: Coherent (temporary transfer buffers)
void MappedBuffer::createStagingBuffer(VkDeviceSize size, void* data)
{
    create(VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, size, data);
}

// Uniform: Coherent (frequently updated shader data)
void MappedBuffer::createUniformBuffer(VkDeviceSize size, void* data)
{
    create(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, size, data);

    resourceBinding().descriptorType_ = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    resourceBinding().buffer_ = buffer_;
    resourceBinding().bufferSize_ = dataSize_;
    resourceBinding().descriptorCount_ = 1;
    initializeBufferResource(buffer_, dataSize_);
}

void MappedBuffer::updateData(const void* data, VkDeviceSize size, VkDeviceSize offset)
{
    if (!mapped_ || !data) {
        return;
    }

    if (offset + size > dataSize_) {
        // Handle error - data exceeds buffer bounds
        return;
    }

    uint8_t* dst = static_cast<uint8_t*>(mapped_) + offset;
    memcpy(dst, data, size);

    // Flush if memory is not coherent
    if ((memPropFlags_ & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0) {
        flush();
    }
}

void MappedBuffer::updateBinding(VkDescriptorSetLayoutBinding& binding)
{
    const ResourceBinding& rb = resourceBinding();
    
    binding.descriptorType = rb.descriptorType_;
    binding.descriptorCount = rb.descriptorCount_;
    binding.pImmutableSamplers = nullptr;
    binding.stageFlags = 0; // Will be set by shader reflection
}

void MappedBuffer::updateWrite(VkWriteDescriptorSet& write)
{
    const ResourceBinding& rb = resourceBinding();
    
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext = nullptr;
    write.dstSet = VK_NULL_HANDLE; // Will be set by DescriptorSet::create()
    write.dstBinding = 0;          // Will be set by DescriptorSet::create()
    write.dstArrayElement = 0;
    write.descriptorType = rb.descriptorType_;
    write.descriptorCount = rb.descriptorCount_;
    write.pBufferInfo = &rb.bufferInfo_;
    write.pImageInfo = nullptr;
    write.pTexelBufferView = nullptr;
}

} // namespace hlab
