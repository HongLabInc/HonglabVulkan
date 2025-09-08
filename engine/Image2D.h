#pragma once

#include "ResourceBinding.h"
#include <string>
#include <vulkan/vulkan.h>

namespace hlab {

using namespace std;

class Context;

class Image2D
{
  public:
    Image2D(Context& ctx);
    Image2D(const Image2D&) = delete;
    Image2D(Image2D&& other) noexcept;
    Image2D& operator=(const Image2D&) = delete;
    Image2D& operator=(Image2D&&) = delete;
    ~Image2D();

    void createFromPixelData(unsigned char* pixels, int w, int h, int c, bool sRGB);
    void createSolid(int width, int height, uint8_t rgba[4]);
    void createTextureFromKtx2(string filename, bool isCubemap);
    void createTextureFromImage(string filename, bool isCubemap, bool sRGB);
    void createRGBA32F(uint32_t width, uint32_t height);
    void createRGBA16F(uint16_t width, uint32_t height);
    void createMsaaColorBuffer(uint16_t width, uint32_t height, VkSampleCountFlagBits sampleCount);
    void createGeneralStorage(uint16_t width, uint32_t height);
    void createShadow(uint32_t width, uint32_t height); // Create shadow map depth texture
    void createDepthBuffer(uint32_t width, uint32_t height, VkSampleCountFlagBits sampleCount);
    void createImage(VkFormat format, uint32_t width, uint32_t height,
                     VkSampleCountFlagBits sampleCount, VkImageUsageFlags usage,
                     VkImageAspectFlags aspectMask, uint32_t mipLevels, uint32_t arrayLayers,
                     VkImageCreateFlags flags, VkImageViewType viewType);
    void cleanup();

    auto image() const -> VkImage;
    auto view() const -> VkImageView;
    auto attachmentView() const -> VkImageView;  // For depth-stencil attachment usage (both aspects)
    auto width() const -> uint32_t;
    auto height() const -> uint32_t;

    void updateUsageFlags(VkImageUsageFlags usageFlags)
    {
        usageFlags_ |= usageFlags;
    }

    void setSampler(VkSampler sampler)
    {
        resourceBinding_.setSampler(sampler);
    }

    // Primary transition method with automatic ResourceBinding updates
    void transitionTo(VkCommandBuffer cmd, VkAccessFlags2 newAccess, VkImageLayout newLayout,
                      VkPipelineStageFlags2 newStage)
    {
        resourceBinding_.barrierHelper_.transitionTo(cmd, newAccess, newLayout, newStage);
        updateResourceBindingAfterTransition();
    }

    // Convenience transition methods - all use the same pattern
    void transitionToColorAttachment(VkCommandBuffer cmd)
    {
        resourceBinding_.barrierHelper_.transitionTo(
            cmd, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
        updateResourceBindingAfterTransition();
    }

    void transitionToTransferSrc(VkCommandBuffer cmd)
    {
        resourceBinding_.barrierHelper_.transitionTo(cmd, VK_ACCESS_2_TRANSFER_READ_BIT,
                                                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                                     VK_PIPELINE_STAGE_2_TRANSFER_BIT);
        updateResourceBindingAfterTransition();
    }

    void transitionToTransferDst(VkCommandBuffer cmd)
    {
        resourceBinding_.barrierHelper_.transitionTo(cmd, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                     VK_PIPELINE_STAGE_2_TRANSFER_BIT);
        updateResourceBindingAfterTransition();
    }

    void transitionToShaderRead(VkCommandBuffer cmd)
    {
        resourceBinding_.barrierHelper_.transitionTo(cmd, VK_ACCESS_2_SHADER_READ_BIT,
                                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                     VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
        updateResourceBindingAfterTransition();
    }

    void transitionToDepthStencilAttachment(VkCommandBuffer cmd)
    {
        resourceBinding_.barrierHelper_.transitionTo(cmd, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                                     VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                                     VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT);
        updateResourceBindingAfterTransition();
    }

    void transitionToGeneral(VkCommandBuffer cmd, VkAccessFlags2 accessFlags,
                             VkPipelineStageFlags2 stageFlags)
    {
        resourceBinding_.barrierHelper_.transitionTo(cmd, accessFlags, VK_IMAGE_LAYOUT_GENERAL,
                                                     stageFlags);
        updateResourceBindingAfterTransition();
    }

    auto resourceBinding() -> ResourceBinding&
    {
        return resourceBinding_;
    }

    // Direct access to barrier helper for advanced usage
    auto barrierHelper() -> BarrierHelper&
    {
        return resourceBinding_.barrierHelper_;
    }

  protected:
    Context& ctx_;

  private:
    VkImage image_{VK_NULL_HANDLE};
    VkDeviceMemory memory_{VK_NULL_HANDLE};
    VkImageView imageView_{VK_NULL_HANDLE};
    VkImageView depthStencilView_{VK_NULL_HANDLE};  // For depth-stencil attachment usage (both aspects)
    VkFormat format_{VK_FORMAT_UNDEFINED};
    uint32_t width_{0};
    uint32_t height_{0};

    VkImageUsageFlags usageFlags_{0};
    ResourceBinding resourceBinding_;

    // Helper method to update ResourceBinding after layout transitions
    void updateResourceBindingAfterTransition()
    {
        VkImageLayout currentLayout = resourceBinding_.barrierHelper_.currentLayout();

        if (currentLayout == VK_IMAGE_LAYOUT_GENERAL) {
            // General layout is used for storage images
            resourceBinding_.descriptorType_ = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            resourceBinding_.imageInfo_.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        } else if (currentLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            // Shader read-only layout is used for sampled images
            if (resourceBinding_.sampler_ != VK_NULL_HANDLE) {
                resourceBinding_.descriptorType_ = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            } else {
                resourceBinding_.descriptorType_ = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            }
            resourceBinding_.imageInfo_.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        } else if (currentLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL ||
                   currentLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            // Attachment layouts are typically used for input attachments when used in descriptors
            resourceBinding_.descriptorType_ = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
            resourceBinding_.imageInfo_.imageLayout = currentLayout;
        } else {
            // For other layouts, default to storage image with general layout capability
            resourceBinding_.descriptorType_ = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            resourceBinding_.imageInfo_.imageLayout = currentLayout;
        }

        // Update the image info
        resourceBinding_.imageInfo_.imageView = imageView_;
        resourceBinding_.imageInfo_.sampler = resourceBinding_.sampler_;
    }

    // Helper method to create depth-stencil attachment view for dual-aspect usage
    void createDepthStencilAttachmentView();
};

} // namespace hlab