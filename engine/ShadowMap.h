#pragma once

#include "Context.h"
#include "ResourceBinding.h"

namespace hlab {

class ShadowMap
{
  public:
    ShadowMap(Context& ctx) : ctx_(ctx)
    {
        const VkDevice device = ctx_.device();

        // Create shadow map image
        VkImageCreateInfo imageCI{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imageCI.imageType = VK_IMAGE_TYPE_2D;
        imageCI.format = format_;
        imageCI.extent = {width_, height_, 1};
        imageCI.mipLevels = 1;
        imageCI.arrayLayers = 1;
        imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        check(vkCreateImage(device, &imageCI, nullptr, &image_));

        // Allocate memory
        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(device, image_, &memReqs);

        VkMemoryAllocateInfo memAlloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        memAlloc.allocationSize = memReqs.size;
        memAlloc.memoryTypeIndex =
            ctx_.getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        check(vkAllocateMemory(device, &memAlloc, nullptr, &memory_));
        check(vkBindImageMemory(device, image_, memory_, 0));

        // Create image view
        VkImageViewCreateInfo viewCI{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewCI.image = image_;
        viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewCI.format = format_;
        viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewCI.subresourceRange.baseMipLevel = 0;
        viewCI.subresourceRange.levelCount = 1;
        viewCI.subresourceRange.baseArrayLayer = 0;
        viewCI.subresourceRange.layerCount = 1;
        check(vkCreateImageView(device, &viewCI, nullptr, &imageView_));

        // Create sampler for shadow map sampling
        VkSamplerCreateInfo samplerCI{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerCI.magFilter = VK_FILTER_LINEAR;
        samplerCI.minFilter = VK_FILTER_LINEAR;
        samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCI.borderColor =
            VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE; // Outside shadow map = fully lit
        
        // Use regular sampler for macOS compatibility - shader does manual depth comparison
        samplerCI.compareEnable = VK_FALSE;
        samplerCI.compareOp = VK_COMPARE_OP_NEVER;
        check(vkCreateSampler(device, &samplerCI, nullptr, &sampler_));

        resourceBinding_.image_ = image_;
        resourceBinding_.imageView_ = imageView_;
        resourceBinding_.sampler_ = sampler_;
        resourceBinding_.descriptorType_ = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        resourceBinding_.descriptorCount_ = 1;
        resourceBinding_.update();
        resourceBinding_.barrierHelper().update(image_, format_, 1, 1);
    }

    ~ShadowMap()
    {
        const VkDevice device = ctx_.device();

        if (sampler_ != VK_NULL_HANDLE) {
            vkDestroySampler(device, sampler_, nullptr);
            sampler_ = VK_NULL_HANDLE;
        }

        if (imageView_ != VK_NULL_HANDLE) {
            vkDestroyImageView(device, imageView_, nullptr);
            imageView_ = VK_NULL_HANDLE;
        }

        if (image_ != VK_NULL_HANDLE) {
            vkDestroyImage(device, image_, nullptr);
            image_ = VK_NULL_HANDLE;
        }

        if (memory_ != VK_NULL_HANDLE) {
            vkFreeMemory(device, memory_, nullptr);
            memory_ = VK_NULL_HANDLE;
        }
    }

    auto image() const -> VkImage
    {
        return image_;
    }

    auto imageView() const -> VkImageView
    {
        return imageView_;
    }

    auto width() const -> uint32_t
    {
        return width_;
    }

    auto height() const -> uint32_t
    {
        return height_;
    }

    auto resourceBinding() -> ResourceBinding&
    {
        return resourceBinding_;
    }

    auto usesComparisonSamplers() const -> bool
    {
        return ctx_.supportsComparisonSamplers();
    }

  private:
    Context& ctx_;
    VkImage image_{VK_NULL_HANDLE};
    VkDeviceMemory memory_{VK_NULL_HANDLE};
    VkImageView imageView_{VK_NULL_HANDLE};
    VkSampler sampler_{VK_NULL_HANDLE};
    uint32_t width_ = 2048 * 2; // Shadow map resolution
    uint32_t height_ = 2048 * 2;
    VkFormat format_ = VK_FORMAT_D16_UNORM;

    ResourceBinding resourceBinding_;
};

} // namespace hlab