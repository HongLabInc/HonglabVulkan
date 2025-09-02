#pragma once

#include "Context.h"
#include "ResourceBinding.h"

namespace hlab {

class DepthStencil // TODO: hlab::Image or texture
{
  public:
    DepthStencil(Context& ctx) : ctx_(ctx)
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

        // Initialize ResourceBinding following ShadowMap pattern
        resourceBinding_.image_ = image;
        resourceBinding_.imageView_ = samplerView; // Use depth-only view for sampling
        resourceBinding_.descriptorType_ = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        resourceBinding_.descriptorCount_ = 1;
        resourceBinding_.update();
        resourceBinding_.barrierHelper().update(image, ctx_.depthFormat(), 1, 1);
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

    // Primary transition method with automatic ResourceBinding updates
    void transitionTo(VkCommandBuffer cmd, VkAccessFlags2 newAccess, VkImageLayout newLayout,
                      VkPipelineStageFlags2 newStage)
    {
        resourceBinding_.barrierHelper().transitionTo(cmd, newAccess, newLayout, newStage);
        updateResourceBindingAfterTransition();
    }

    // Convenience transition methods for depth buffer
    void transitionToDepthStencilAttachment(VkCommandBuffer cmd)
    {
        resourceBinding_.barrierHelper().transitionTo(
            cmd, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, 
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT);
        updateResourceBindingAfterTransition();
    }

    void transitionToShaderRead(VkCommandBuffer cmd)
    {
        resourceBinding_.barrierHelper().transitionTo(cmd, VK_ACCESS_2_SHADER_READ_BIT,
                                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                     VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
        updateResourceBindingAfterTransition();
    }

    void setSampler(VkSampler sampler)
    {
        resourceBinding_.setSampler(sampler);
    }

    auto resourceBinding() -> ResourceBinding&
    {
        return resourceBinding_;
    }

    // Direct access to barrier helper for advanced usage
    auto barrierHelper() -> BarrierHelper&
    {
        return resourceBinding_.barrierHelper();
    }

    VkImage image{VK_NULL_HANDLE};
    VkDeviceMemory memory{VK_NULL_HANDLE};
    VkImageView view{VK_NULL_HANDLE};        // For depth-stencil attachment (both aspects)
    VkImageView samplerView{VK_NULL_HANDLE}; // For shader sampling (depth only)

  private:
    Context& ctx_;
    ResourceBinding resourceBinding_;

    // Helper method to update ResourceBinding after layout transitions
    void updateResourceBindingAfterTransition()
    {
        VkImageLayout currentLayout = resourceBinding_.barrierHelper().currentLayout();

        if (currentLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            // Depth buffer being used for shader sampling
            if (resourceBinding_.sampler_ != VK_NULL_HANDLE) {
                resourceBinding_.descriptorType_ = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            } else {
                resourceBinding_.descriptorType_ = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            }
            resourceBinding_.imageInfo_.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        } else if (currentLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            // Depth buffer being used as attachment
            resourceBinding_.descriptorType_ = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
            resourceBinding_.imageInfo_.imageLayout = currentLayout;
        } else {
            // For other layouts, default to sampled image
            resourceBinding_.descriptorType_ = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            resourceBinding_.imageInfo_.imageLayout = currentLayout;
        }

        // Update the image info with the depth-only view for sampling
        resourceBinding_.imageInfo_.imageView = samplerView;
        resourceBinding_.imageInfo_.sampler = resourceBinding_.sampler_;
    }
};

} // namespace hlab
