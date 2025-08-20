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
    void createTextureFromKtx2(string filename, bool isCubemap);
    void createTextureFromImage(string filename, bool isCubemap, bool sRGB);
    void createRGBA32F(uint32_t width, uint32_t height);
    void createRGBA16F(uint16_t width, uint32_t height);
    void createMsaaColorBuffer(uint16_t width, uint32_t height, VkSampleCountFlagBits sampleCount);
    void createGeneralStorage(uint16_t width, uint32_t height);
    void createImage(VkFormat format, uint32_t width, uint32_t height,
                     VkSampleCountFlagBits sampleCount, VkImageUsageFlags usage,
                     VkImageAspectFlags aspectMask, uint32_t mipLevels, uint32_t arrayLayers,
                     VkImageCreateFlags flags, VkImageViewType viewType);
    void cleanup();

    auto image() const -> VkImage;
    auto view() -> VkImageView;
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

    auto resourceBinding() -> ResourceBinding&
    {
        return resourceBinding_;
    }

    // Simplified barrier transition methods using the internal BarrierHelper
    void transitionLayout(VkCommandBuffer cmd, VkAccessFlags2 newAccess, VkImageLayout newLayout,
                          VkPipelineStageFlags2 newStage)
    {
        resourceBinding_.barrierHelper().transitionTo(cmd, newAccess, newLayout, newStage);
    }

    void transitionToColorAttachment(VkCommandBuffer cmd)
    {
        resourceBinding_.barrierHelper().transitionTo(
            cmd, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
    }

    void transitionToTransferSrc(VkCommandBuffer cmd)
    {
        resourceBinding_.barrierHelper().transitionTo(cmd, VK_ACCESS_2_TRANSFER_READ_BIT,
                                                      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                                      VK_PIPELINE_STAGE_2_TRANSFER_BIT);
    }

    void transitionToTransferDst(VkCommandBuffer cmd)
    {
        resourceBinding_.barrierHelper().transitionTo(cmd, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                      VK_PIPELINE_STAGE_2_TRANSFER_BIT);
    }

    void transitionToShaderRead(VkCommandBuffer cmd)
    {
        resourceBinding_.barrierHelper().transitionTo(cmd, VK_ACCESS_2_SHADER_READ_BIT,
                                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                      VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
    }

    // Direct access to barrier helper for advanced usage
    auto barrierHelper() -> BarrierHelper&
    {
        return resourceBinding_.barrierHelper();
    }

  private:
    Context& ctx_;

    VkImage image_{VK_NULL_HANDLE};
    VkDeviceMemory memory_{VK_NULL_HANDLE};
    VkImageView imageView_{VK_NULL_HANDLE};
    VkFormat format_{VK_FORMAT_UNDEFINED};
    uint32_t width_{0};
    uint32_t height_{0};

    VkImageUsageFlags usageFlags_{0};
    ResourceBinding resourceBinding_;
};

} // namespace hlab