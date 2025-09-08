#pragma once

#include "Context.h"
#include "Logger.h"
#include "ResourceBinding.h"
#include "Pipeline.h"
#include <vulkan/vulkan.h>
#include <optional>

namespace hlab {

class DescriptorSet
{
  public:
    DescriptorSet() = default; // Do not destroy handles (layout_, set_, etc.)

    // 안내: 리소스 해제에 대한 책임이 없기 때문에 Context를 멤버로 갖고 있을 필요가 없음
    void create(Context& ctx, const vector<reference_wrapper<ResourceBinding>>& resourceBindings)
    {
        vector<VkDescriptorSetLayoutBinding> layoutBindings(resourceBindings.size());
        for (size_t i = 0; i < resourceBindings.size(); i++) {
            const ResourceBinding& rb = resourceBindings[i].get();
            layoutBindings[i].binding = uint32_t(i);
            layoutBindings[i].descriptorType = rb.descriptorType_;
            layoutBindings[i].descriptorCount = rb.descriptorCount_;
            layoutBindings[i].pImmutableSamplers = nullptr;
            layoutBindings[i].stageFlags = 0;
            // Note: stageFlags is not used to retrieve layout below.
            //       see BindingEqual and BindingHash in VulkanTools.h for details.
        }

        // 1. bindings로부터 layout을 찾는다.
        VkDescriptorSetLayout layout = ctx.descriptorPool().descriptorSetLayout(layoutBindings);

        layoutBindings = ctx.descriptorPool().layoutToBindings(layout);
        // Note: 여기서 stageFlags가 포함된 완전한 layoutBindings을 가져옵니다.

        // Update stageFlags of resourceBindings (셰이더에서 결정한 것)
        for (size_t i = 0; i < resourceBindings.size(); i++) {
            resourceBindings[i].get().stageFlags = layoutBindings[i].stageFlags;
        }

        descriptorSet_ = ctx.descriptorPool().allocateDescriptorSet(layout);

        vector<VkWriteDescriptorSet> descriptorWrites(layoutBindings.size());
        for (size_t bindingIndex = 0; bindingIndex < layoutBindings.size(); ++bindingIndex) {
            const ResourceBinding& rb = resourceBindings[bindingIndex].get();
            VkWriteDescriptorSet& write = descriptorWrites[bindingIndex];

            // rb.updateWrite(write); // <- 상속을 이용해서 이런 구조로 변경 예정

            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.pNext = nullptr;
            write.dstSet = descriptorSet_;
            write.dstBinding = layoutBindings[bindingIndex].binding;
            write.dstArrayElement = 0; // <- Bindless Texture의 인덱스로 지정
            write.descriptorType = layoutBindings[bindingIndex].descriptorType;
            // write.descriptorCount = layoutBindings[bindingIndex].descriptorCount;
            write.descriptorCount = 1; // <- 임시로 하나만 지정
            write.pBufferInfo = rb.buffer_ ? &rb.bufferInfo_ : nullptr;
            write.pImageInfo = rb.image_ ? &rb.imageInfo_ : nullptr;
            write.pTexelBufferView = nullptr; /* Not implememted */
        }

        if (!descriptorWrites.empty()) {
            vkUpdateDescriptorSets(ctx.device(), static_cast<uint32_t>(descriptorWrites.size()),
                                   descriptorWrites.data(), 0, nullptr);
        }
    }

    auto handle() const -> const VkDescriptorSet&
    {
        if (descriptorSet_ == VK_NULL_HANDLE) {
            exitWithMessage("DescriptorSet is empty.");
        }

        return descriptorSet_;
    }

  private:
    VkDescriptorSet descriptorSet_{VK_NULL_HANDLE};
};

} // namespace hlab