#pragma once

#include "Context.h"
#include "BarrierHelper.h"

namespace hlab {

class DepthStencil // TODO: hlab::Image or texture
{
  public:
    DepthStencil(Context& ctx) : ctx_(ctx), barrierHelper_(image)
    {
    }

    ~DepthStencil()
    {
        cleanup();
    }

    void create(uint32_t width, uint32_t height, VkSampleCountFlagBits msaaSamples)
    {
        VkImageCreateInfo imageCI{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imageCI.imageType = VK_IMAGE_TYPE_2D;
        imageCI.format = ctx_.depthFormat();
        imageCI.extent = {width, height, 1};
        imageCI.mipLevels = 1;
        imageCI.arrayLayers = 1;
        imageCI.samples = msaaSamples;
        imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        check(vkCreateImage(ctx_.device(), &imageCI, nullptr, &image));

        VkMemoryAllocateInfo memAlloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(ctx_.device(), image, &memReqs);
        memAlloc.allocationSize = memReqs.size;
        memAlloc.memoryTypeIndex =
            ctx_.getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        check(vkAllocateMemory(ctx_.device(), &memAlloc, nullptr, &memory));
        check(vkBindImageMemory(ctx_.device(), image, memory, 0));

        // Create depth-stencil view for rendering (includes both depth and stencil aspects)
        VkImageViewCreateInfo depthStencilViewCI{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        depthStencilViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
        depthStencilViewCI.format = ctx_.depthFormat();
        depthStencilViewCI.subresourceRange = {};
        depthStencilViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

        if (ctx_.depthFormat() >= VK_FORMAT_D16_UNORM_S8_UINT) {
            depthStencilViewCI.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
        depthStencilViewCI.subresourceRange.baseMipLevel = 0;
        depthStencilViewCI.subresourceRange.levelCount = 1;
        depthStencilViewCI.subresourceRange.baseArrayLayer = 0;
        depthStencilViewCI.subresourceRange.layerCount = 1;
        depthStencilViewCI.image = image;
        check(vkCreateImageView(ctx_.device(), &depthStencilViewCI, nullptr, &view));

        // Create depth-only view for shader sampling (only depth aspect)
        VkImageViewCreateInfo depthOnlyViewCI{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        depthOnlyViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
        depthOnlyViewCI.format = ctx_.depthFormat();
        depthOnlyViewCI.subresourceRange = {};
        depthOnlyViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT; // Only depth
        depthOnlyViewCI.subresourceRange.baseMipLevel = 0;
        depthOnlyViewCI.subresourceRange.levelCount = 1;
        depthOnlyViewCI.subresourceRange.baseArrayLayer = 0;
        depthOnlyViewCI.subresourceRange.layerCount = 1;
        depthOnlyViewCI.image = image;
        check(vkCreateImageView(ctx_.device(), &depthOnlyViewCI, nullptr, &samplerView));
    }

    void cleanup()
    {
        if (samplerView != VK_NULL_HANDLE) {
            vkDestroyImageView(ctx_.device(), samplerView, nullptr);
            samplerView = VK_NULL_HANDLE;
        }
        if (view != VK_NULL_HANDLE) {
            vkDestroyImageView(ctx_.device(), view, nullptr);
            view = VK_NULL_HANDLE;
        }
        if (memory != VK_NULL_HANDLE) {
            vkFreeMemory(ctx_.device(), memory, nullptr);
            memory = VK_NULL_HANDLE;
        }
        if (image != VK_NULL_HANDLE) {
            vkDestroyImage(ctx_.device(), image, nullptr);
            image = VK_NULL_HANDLE;
        }
    }

    VkImage image{VK_NULL_HANDLE};
    VkDeviceMemory memory{VK_NULL_HANDLE};
    VkImageView view{VK_NULL_HANDLE};        // For depth-stencil attachment (both aspects)
    VkImageView samplerView{VK_NULL_HANDLE}; // For shader sampling (depth only)
    BarrierHelper barrierHelper_;

  private:
    Context& ctx_;
};

} // namespace hlab
