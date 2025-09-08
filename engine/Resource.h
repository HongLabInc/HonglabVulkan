#pragma once

#include "Logger.h"
#include "VulkanTools.h"

namespace hlab {

using namespace std;

class Resource
{
  public:
    virtual uint32_t descriptorCount() = 0;
    virtual VkDescriptorType descriptorType(bool forCompute) = 0;
    virtual void setStageFlags(VkShaderStageFlags& flag)
    {
        stageFlags = flag;
    }
    virtual void addImageUsageFlags(VkImageUsageFlags usageFlags)
    {
        exitWithMessage("Override virtual void addUsageFlags(VkImageUsageFlags)");
    }
    virtual void addBufferUsageFlags(VkBufferUsageFlags usageFlags)
    {
        exitWithMessage("Override virtual void addUsageFlagsVkBufferUsageFlags)");
    }
    // 안내: 두 함수들이 동시에 오버라이드 될 수는 없기 때문에 가상 함수로 선언하지 않고
    //      실제로 사용이 되면 오류를 발생시키는 방식으로 구현

    virtual void updateBinding(VkDescriptorSetLayoutBinding& binding)
    {
        //     const ResourceBinding& rb = resourceBindings[i].get();
        //     layoutBindings[i].binding = uint32_t(i);
        //     layoutBindings[i].descriptorType = rb.descriptorType_;
        //     layoutBindings[i].descriptorCount = rb.descriptorCount_;
        //     layoutBindings[i].pImmutableSamplers = nullptr;
        //     layoutBindings[i].stageFlags = 0;
        //     // Note: stageFlags is not used to retrieve layout below.
        //     //       see BindingEqual and BindingHash in VulkanTools.h for details.
    }

    virtual void updateWrite(VkWriteDescriptorSet& write)
    {
        //    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        //    write.pNext = nullptr;
        //    write.dstSet = descriptorSet_;
        //    write.dstBinding = layoutBindings[bindingIndex].binding;
        //    write.dstArrayElement = 0; // <- Bindless Texture의 인덱스로 지정
        //    write.descriptorType = layoutBindings[bindingIndex].descriptorType;
        //    // write.descriptorCount = layoutBindings[bindingIndex].descriptorCount;
        //    write.descriptorCount = 1; // <- 임시로 하나만 지정
        //    write.pBufferInfo = rb.buffer_ ? &rb.bufferInfo_ : nullptr;
        //    write.pImageInfo = rb.image_ ? &rb.imageInfo_ : nullptr;
        //    write.pTexelBufferView = nullptr; /* Not implememted */
    }

  protected:
    string name_{};
    VkShaderStageFlags stageFlags{0};
    VkImageUsageFlags usageFlags_{0};
};

} // namespace hlab