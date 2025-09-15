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
    void create(Context& ctx, const VkDescriptorSetLayout& layout,
                const vector<reference_wrapper<Resource>>& resources)
    {
        resources_ = resources;

        const vector<VkDescriptorSetLayoutBinding> layoutBindings =
            ctx.descriptorPool().layoutToBindings(layout);

        descriptorSet_ = ctx.descriptorPool().allocateDescriptorSet(layout);

        vector<VkWriteDescriptorSet> descriptorWrites(layoutBindings.size());
        for (size_t i = 0; i < layoutBindings.size(); ++i) {
            VkWriteDescriptorSet& write = descriptorWrites[i];

            resources[i].get().updateWrite(layoutBindings[i], write);

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

    auto resources() const -> const vector<reference_wrapper<Resource>>&
    {
        return resources_;
    }

  private:
    VkDescriptorSet descriptorSet_{VK_NULL_HANDLE};
    vector<reference_wrapper<Resource>> resources_;
};

} // namespace hlab