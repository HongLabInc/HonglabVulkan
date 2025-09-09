#pragma once

#include "Sampler.h"
#include "Resource.h"
#include <optional>
#include <functional>
#include <string>
#include <vulkan/vulkan.h>

namespace hlab {

using namespace std;

class Context;

class Image2D : public Resource
{
  public:
    Image2D(Context& ctx);
    Image2D(const Image2D&) = delete;
    Image2D(Image2D&&) = delete;
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
    void cleanup() override;

    // Implement required Resource methods
    void updateBinding(VkDescriptorSetLayoutBinding& binding) override;
    void updateWrite(VkWriteDescriptorSet& write) override;

    auto image() const -> VkImage;
    auto view() const -> VkImageView;
    auto attachmentView() const -> VkImageView; // For depth-stencil attachment usage (both aspects)
    auto width() const -> uint32_t;
    auto height() const -> uint32_t;
    auto format() const -> VkFormat;

    void updateUsageFlags(VkImageUsageFlags usageFlags)
    {
        usageFlags_ |= usageFlags;
    }

    // Legacy interface for backward compatibility
    auto resourceBinding() -> ResourceBinding&
    {
        return Resource::resourceBinding();
    }

    // Direct access to barrier helper for advanced usage
    auto barrierHelper() -> BarrierHelper&
    {
        return Resource::barrierHelper();
    }

    void updateImageInfo(VkDescriptorImageInfo& imageInfo)
    {
        const auto& rb = resourceBinding();

        imageInfo.sampler = rb.imageInfo_.sampler;
        imageInfo.imageView = imageView_;
        imageInfo.imageLayout = rb.imageInfo_.imageLayout;
    }

  private:
    VkImage image_{VK_NULL_HANDLE};
    VkDeviceMemory memory_{VK_NULL_HANDLE};
    VkImageView imageView_{VK_NULL_HANDLE};
    VkImageView depthStencilView_{
        VK_NULL_HANDLE}; // For depth-stencil attachment usage (both aspects)
    VkFormat format_{VK_FORMAT_UNDEFINED};
    uint32_t width_{0};
    uint32_t height_{0};

    VkImageUsageFlags usageFlags_{0};

    void updateResourceBindingAfterTransition();
    void createDepthStencilAttachmentView();
};

} // namespace hlab