#pragma once

#include "Context.h"
#include "Logger.h"
#include "Resource.h"
#include "Pipeline.h"
#include <vulkan/vulkan.h>
#include <optional>

namespace hlab {

class DescriptorSet
{
  public:
    DescriptorSet() = default; // Do not destroy handles (layout_, set_, etc.)

    // 안내: 리소스 해제에 대한 책임이 없기 때문에 Context를 멤버로 갖고 있을 필요가 없음
    void create(Context& ctx, const vector<reference_wrapper<Resource>>& resources)
    {
        vector<VkDescriptorSetLayoutBinding> layoutBindings(resources.size());
        for (size_t i = 0; i < resources.size(); i++) {
            resources[i].get().updateBinding(layoutBindings[i]);
            layoutBindings[i].binding = uint32_t(i);
        }

        // 1. bindings로부터 layout을 찾는다.
        VkDescriptorSetLayout layout = ctx.descriptorPool().descriptorSetLayout(layoutBindings);

        layoutBindings = ctx.descriptorPool().layoutToBindings(layout);
        // Note: 여기서 stageFlags가 포함된 완전한 layoutBindings을 가져옵니다.

        // Update stageFlags of resourceBindings (셰이더에서 결정한 것)
        for (size_t i = 0; i < resources.size(); i++) {
            resources[i].get().resourceBinding().stageFlags = layoutBindings[i].stageFlags;
        }

        descriptorSet_ = ctx.descriptorPool().allocateDescriptorSet(layout);

        vector<VkWriteDescriptorSet> descriptorWrites(layoutBindings.size());
        for (size_t i = 0; i < layoutBindings.size(); ++i) {
            VkWriteDescriptorSet& write = descriptorWrites[i];
            
            // Call the polymorphic updateWrite method
            resources[i].get().updateWrite(write);
            
            // Override fields that must be set by DescriptorSet
            write.dstSet = descriptorSet_;
            write.dstBinding = layoutBindings[i].binding;
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