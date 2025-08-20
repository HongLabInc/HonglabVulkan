#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <functional>

namespace hlab {

// Modern barrier helper following industry standards
class BarrierHelper
{
    friend class ResourceBinding;

  public:
    BarrierHelper()
    {
    }

    BarrierHelper(const VkImage& image) : image_(image)
    {
    }

    // Move constructor
    BarrierHelper(BarrierHelper&& other) noexcept
        : image_(other.image_), format_(other.format_), mipLevels_(other.mipLevels_),
          arrayLayers_(other.arrayLayers_), currentLayout_(other.currentLayout_),
          currentAccess_(other.currentAccess_), currentStage_(other.currentStage_)
    {
        // Reset the moved-from object to a safe state
        other.format_ = VK_FORMAT_UNDEFINED;
        other.mipLevels_ = 1;
        other.arrayLayers_ = 1;
        other.currentLayout_ = VK_IMAGE_LAYOUT_UNDEFINED;
        other.currentAccess_ = VK_ACCESS_2_NONE;
        other.currentStage_ = VK_PIPELINE_STAGE_2_NONE;
    }

    // Move assignment operator
    BarrierHelper& operator=(BarrierHelper&& other) noexcept
    {
        if (this != &other) {
            image_ = other.image_;
            format_ = other.format_;
            mipLevels_ = other.mipLevels_;
            arrayLayers_ = other.arrayLayers_;
            currentLayout_ = other.currentLayout_;
            currentAccess_ = other.currentAccess_;
            currentStage_ = other.currentStage_;

            // Reset the moved-from object to a safe state
            other.format_ = VK_FORMAT_UNDEFINED;
            other.mipLevels_ = 1;
            other.arrayLayers_ = 1;
            other.currentLayout_ = VK_IMAGE_LAYOUT_UNDEFINED;
            other.currentAccess_ = VK_ACCESS_2_NONE;
            other.currentStage_ = VK_PIPELINE_STAGE_2_NONE;
        }
        return *this;
    }

    void update(VkImage image, VkFormat format, uint32_t mipLevels, uint32_t arrayLayers)
    {
        image_ = image;
        format_ = format;
        mipLevels_ = mipLevels;
        arrayLayers_ = arrayLayers;
    }

    auto currentLayout() -> VkImageLayout&
    {
        return currentLayout_;
    }
    auto currentAccess() -> VkAccessFlags2&
    {
        return currentAccess_;
    }
    auto currentStage() -> VkPipelineStageFlags2&
    {
        return currentStage_;
    }
    auto format() -> VkFormat&
    {
        return format_;
    }
    auto mipLevels() -> uint32_t&
    {
        return mipLevels_;
    }
    auto arrayLayers() -> uint32_t&
    {
        return arrayLayers_;
    }

    void transitionTo(VkCommandBuffer cmd, VkAccessFlags2 newAccess, VkImageLayout newLayout,
                      VkPipelineStageFlags2 newStage, uint32_t baseMipLevel = 0,
                      uint32_t levelCount = VK_REMAINING_MIP_LEVELS, uint32_t baseArrayLayer = 0,
                      uint32_t layerCount = VK_REMAINING_ARRAY_LAYERS)
    {
        // Validate that image is valid before proceeding
        if (image_ == VK_NULL_HANDLE) {
            return;
        }

        // Resolve VK_REMAINING_* values
        uint32_t actualLevelCount =
            (levelCount == VK_REMAINING_MIP_LEVELS) ? (mipLevels_ - baseMipLevel) : levelCount;
        uint32_t actualLayerCount = (layerCount == VK_REMAINING_ARRAY_LAYERS)
                                        ? (arrayLayers_ - baseArrayLayer)
                                        : layerCount;

        // Skip redundant transitions
        if (currentLayout_ == newLayout && currentAccess_ == newAccess && baseMipLevel == 0 &&
            actualLevelCount == mipLevels_ && baseArrayLayer == 0 &&
            actualLayerCount == arrayLayers_) {
            return;
        }

        // Validate transition
        if (!isValidTransition(currentLayout_, newLayout)) {
            return;
        }

        VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        barrier.srcStageMask = (currentStage_ != VK_PIPELINE_STAGE_2_NONE)
                                   ? currentStage_
                                   : VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        barrier.dstStageMask = newStage;
        barrier.srcAccessMask = currentAccess_;
        barrier.dstAccessMask = newAccess;
        barrier.oldLayout = currentLayout_;
        barrier.newLayout = newLayout;
        barrier.image = image_;
        barrier.subresourceRange = {getAspectMask(), baseMipLevel, actualLevelCount, baseArrayLayer,
                                    actualLayerCount};
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        VkDependencyInfo depInfo{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers = &barrier;

        vkCmdPipelineBarrier2(cmd, &depInfo);

        // Update state only if transitioning the entire image
        if (baseMipLevel == 0 && actualLevelCount == mipLevels_ && baseArrayLayer == 0 &&
            actualLayerCount == arrayLayers_) {
            currentLayout_ = newLayout;
            currentAccess_ = newAccess;
            currentStage_ = newStage;
        }
    }

    auto prepareBarrier(VkImageLayout targetLayout, VkAccessFlags2 targetAccess,
                        VkPipelineStageFlags2 targetStage) -> VkImageMemoryBarrier2
    {
        VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        barrier.srcStageMask = (currentStage_ != VK_PIPELINE_STAGE_2_NONE)
                                   ? currentStage_
                                   : VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        barrier.dstStageMask = targetStage;
        barrier.srcAccessMask = currentAccess_;
        barrier.dstAccessMask = targetAccess;
        barrier.oldLayout = currentLayout_;
        barrier.newLayout = targetLayout;
        barrier.image = image_;
        barrier.subresourceRange = {getAspectMask(), 0, mipLevels_, 0, arrayLayers_};
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        currentLayout_ = targetLayout;
        currentAccess_ = targetAccess;
        currentStage_ = targetStage;

        return barrier;
    }

    auto getAspectMask() const -> VkImageAspectFlags
    {
        if (format_ >= VK_FORMAT_D16_UNORM && format_ <= VK_FORMAT_D32_SFLOAT_S8_UINT) {
            if (format_ == VK_FORMAT_D32_SFLOAT_S8_UINT || format_ == VK_FORMAT_D24_UNORM_S8_UINT) {
                return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            }
            return VK_IMAGE_ASPECT_DEPTH_BIT;
        }
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }

  private:
    VkImage image_{VK_NULL_HANDLE};
    VkFormat format_{VK_FORMAT_UNDEFINED};
    uint32_t mipLevels_{1};
    uint32_t arrayLayers_{1};

    VkImageLayout currentLayout_ = VK_IMAGE_LAYOUT_UNDEFINED;
    VkAccessFlags2 currentAccess_ = VK_ACCESS_2_NONE;
    VkPipelineStageFlags2 currentStage_ = VK_PIPELINE_STAGE_2_NONE;

    bool isValidTransition(VkImageLayout oldLayout, VkImageLayout newLayout) const
    {
        if (oldLayout == newLayout || oldLayout == VK_IMAGE_LAYOUT_UNDEFINED)
            return true;

        if (oldLayout == VK_IMAGE_LAYOUT_PREINITIALIZED) {
            return newLayout == VK_IMAGE_LAYOUT_GENERAL ||
                   newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        }

        return true;
    }
};

} // namespace hlab