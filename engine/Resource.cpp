#include "Resource.h"

namespace hlab {

Resource::Resource(Context& ctx, Type type) : ctx_(ctx), type_(type)
{
}

// Move constructor implementation
Resource::Resource(Resource&& other) noexcept
    : ctx_(other.ctx_), type_(other.type_), 
      barrierHelper_(std::move(other.barrierHelper_)), 
      resourceBinding_(std::move(other.resourceBinding_))
{
    // Nothing special needed - members handle their own move
}

// Move assignment operator implementation
Resource& Resource::operator=(Resource&& other) noexcept
{
    if (this != &other) {
        // Note: ctx_ is a reference so we can't move it, but it should reference the same context
        type_ = other.type_;
        barrierHelper_ = std::move(other.barrierHelper_);
        resourceBinding_ = std::move(other.resourceBinding_);
    }
    return *this;
}

void Resource::transitionTo(VkCommandBuffer cmd, VkAccessFlags2 newAccess,
                                VkImageLayout newLayout, VkPipelineStageFlags2 newStage)
{
    assertImageType();
    barrierHelper_.transitionTo(cmd, newAccess, newLayout, newStage);
    updateResourceBinding();
}

void Resource::transitionToColorAttachment(VkCommandBuffer cmd)
{
    assertImageType();
    barrierHelper_.transitionTo(cmd, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
    updateResourceBinding();
}

void Resource::transitionToTransferSrc(VkCommandBuffer cmd)
{
    assertImageType();
    barrierHelper_.transitionTo(cmd, VK_ACCESS_2_TRANSFER_READ_BIT,
                                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                VK_PIPELINE_STAGE_2_TRANSFER_BIT);
    updateResourceBinding();
}

void Resource::transitionToTransferDst(VkCommandBuffer cmd)
{
    assertImageType();
    barrierHelper_.transitionTo(cmd, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_PIPELINE_STAGE_2_TRANSFER_BIT);
    updateResourceBinding();
}

void Resource::transitionToShaderRead(VkCommandBuffer cmd)
{
    assertImageType();
    barrierHelper_.transitionTo(cmd, VK_ACCESS_2_SHADER_READ_BIT,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
    updateResourceBinding();
}

void Resource::transitionToDepthStencilAttachment(VkCommandBuffer cmd)
{
    assertImageType();
    barrierHelper_.transitionTo(cmd, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT);
    updateResourceBinding();
}

void Resource::transitionToGeneral(VkCommandBuffer cmd, VkAccessFlags2 accessFlags,
                                       VkPipelineStageFlags2 stageFlags)
{
    assertImageType();
    barrierHelper_.transitionTo(cmd, accessFlags, VK_IMAGE_LAYOUT_GENERAL, stageFlags);
    updateResourceBinding();

    resourceBinding_.imageInfo_.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    resourceBinding_.descriptorType_ = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    // Temporary cure before major refactoring
    // Need to check if this is an image and for compute pipeline.
}

void Resource::setSampler(VkSampler sampler)
{
    assertImageType();
    resourceBinding_.setSampler(sampler);
}

void Resource::transitionBuffer(VkCommandBuffer cmd, VkAccessFlags2 srcAccess,
                                    VkAccessFlags2 dstAccess, VkPipelineStageFlags2 srcStage,
                                    VkPipelineStageFlags2 dstStage)
{
    assertBufferType();
    // Buffer barrier implementation would go here
    // This is simpler than image barriers - mainly for compute/transfer sync
    // For now, we'll leave this as a placeholder since the current BarrierHelper
    // is focused on image transitions
}

void Resource::initializeImageResource(VkImage image, VkFormat format, uint32_t mipLevels,
                                           uint32_t arrayLayers)
{
    assertImageType();
    barrierHelper_.update(image, format, mipLevels, arrayLayers);
}

void Resource::initializeBufferResource(VkBuffer buffer, VkDeviceSize size)
{
    assertBufferType();
    // Initialize buffer-specific ResourceBinding data
    resourceBinding_.buffer_ = buffer;
    resourceBinding_.bufferSize_ = size;
    resourceBinding_.update();
}

void Resource::updateResourceBinding()
{
    resourceBinding_.update();
}

void Resource::assertImageType() const
{
    if (type_ != Type::Image) {
        exitWithMessage("Operation only valid for Image resources");
    }
}

void Resource::assertBufferType() const
{
    if (type_ != Type::Buffer) {
        exitWithMessage("Operation only valid for Buffer resources");
    }
}

} // namespace hlab