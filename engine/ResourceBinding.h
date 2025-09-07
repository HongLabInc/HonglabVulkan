#pragma once

#include "VulkanTools.h"
#include "BarrierHelper.h"
#include "Logger.h"
#include <optional>

namespace hlab {

// 메모: 리소스 생성을 모두 자동으로 하면 이 클래스는 제거할 수 있습니다.
//      어떤 리소스가 어떤 파이프라인에서 어떻게 사용될지 모두 알 수 있기 때문입니다.

class ResourceBinding
{
    friend class DescriptorSet;
    friend class Image2D;
    friend class MappedBuffer;
    friend class ShadowMap;
    friend class DepthStencil;
    friend class TextureManager;
    friend class Resource;      // Add Resource as friend
    friend class StorageBuffer; // Add StorageBuffer as friend

  public:
    void update()
    {
        if (buffer_ != VK_NULL_HANDLE) {
            // Handle buffer-based descriptors
            bufferInfo_.buffer = buffer_;
            bufferInfo_.offset = 0;
            bufferInfo_.range = bufferSize_;
            // descriptorType_ should already be set by the calling code
            // (e.g., MappedBuffer::createUniformBuffer sets it to
            // VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
        } else if (image_ && sampler_) {
            descriptorType_ = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            imageInfo_.imageView = imageView_;
            imageInfo_.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo_.sampler = sampler_;
        } else if (image_) {
            descriptorType_ = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            imageInfo_.imageView = imageView_;
            imageInfo_.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        } else {
            exitWithMessage("Neither buffer nor image is ready");
        }
    }

    void setSampler(VkSampler sampler)
    {
        sampler_ = sampler;
        update();
    }

    BarrierHelper& barrierHelper()
    {
        return barrierHelper_;
    }

  private:
    VkImage image_{VK_NULL_HANDLE};
    VkImageView imageView_{VK_NULL_HANDLE};
    VkImageLayout imageLayout_{VK_IMAGE_LAYOUT_UNDEFINED};
    VkSampler sampler_{VK_NULL_HANDLE};

    VkBuffer buffer_{VK_NULL_HANDLE};
    VkDeviceSize bufferSize_{0};

    VkDescriptorType descriptorType_{};
    uint32_t descriptorCount_{};
    VkShaderStageFlags stageFlags{};

    VkDescriptorImageInfo imageInfo_{};
    VkDescriptorBufferInfo bufferInfo_{};
    VkBufferView texelBufferView_ = {VK_NULL_HANDLE};

    BarrierHelper barrierHelper_; // Transition에 사용
    // 주의사항
    // descriptorType_ = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
    // - imageInfo_의 imageView와 sampler 둘 다 설정되어 있어야 합니다.
};

} // namespace hlab