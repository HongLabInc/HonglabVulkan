#pragma once

#include "Context.h"
#include "Image2D.h"
#include "DescriptorSet.h"
#include "Resource.h"
#include "VulkanTools.h"
#include "Sampler.h"
#include <vector>
#include <queue>
#include <string>
#include <memory>

namespace hlab {

using namespace std;

class TextureManager : public Resource
{
    friend class Model;

  public:
    TextureManager(Context& ctx);
    TextureManager(TextureManager&& other) noexcept;
    ~TextureManager();
    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(TextureManager&& other) = delete;
    TextureManager& operator=(const TextureManager&) = delete;

    void cleanup() override;

    void updateBinding(VkDescriptorSetLayoutBinding& binding) override
    {
        // Note: binding.binding will be set by DescriptorSet based on array index
        binding.binding = 0; // This will be overridden by DescriptorSet::create()
        binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount = kMaxTextures_;
        binding.pImmutableSamplers = nullptr;
        binding.stageFlags = 0; // Will be filled by shader reflection
    }

    void updateWrite(VkWriteDescriptorSet& write) override
    {
        static vector<VkDescriptorImageInfo> imageInfos;

        imageInfos.clear();
        imageInfos.resize(textures_.size());
        for (size_t i = 0; i < textures_.size(); ++i) {
            if (!textures_[i]) {
                exitWithMessage("Texture was not created.");
            }

            textures_[i]->updateImageInfo(imageInfos[i]);
        }

        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.pNext = nullptr;
        write.dstSet = VK_NULL_HANDLE; // Will be set by DescriptorSet::create()
        write.dstBinding = 0;          // Will be set by DescriptorSet::create()
        write.dstArrayElement = 0;     // Bindless Texture index
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = uint32_t(textures_.size());
        write.pBufferInfo = nullptr;
        write.pImageInfo = imageInfos.data();
        write.pTexelBufferView = nullptr; // Not implemented yet
    }

  private:
    const uint32_t kMaxTextures_ = 512;

    vector<unique_ptr<Image2D>> textures_;
};

} // namespace hlab
