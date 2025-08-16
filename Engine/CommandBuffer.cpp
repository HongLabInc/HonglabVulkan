#include "CommandBuffer.h"
#include "VulkanTools.h"
#include "Context.h"

namespace hlab {
CommandBuffer::CommandBuffer(VkDevice& device, VkCommandBuffer& handle, VkCommandPool& commandPool,
                             VkQueue& queue, VkCommandBufferLevel level)
    : device_(device), handle_(handle), commandPool_(commandPool), queue_(queue)
{
}

CommandBuffer::CommandBuffer(VkDevice& device, VkCommandPool& commandPool, VkQueue& queue,
                             VkCommandBufferLevel level, bool begin)
    : device_(device), commandPool_(commandPool), queue_(queue)
{
    VkCommandBufferAllocateInfo cmdBufAllocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cmdBufAllocInfo.commandPool = commandPool;
    cmdBufAllocInfo.level = level;
    cmdBufAllocInfo.commandBufferCount = 1;

    check(vkAllocateCommandBuffers(device, &cmdBufAllocInfo, &handle_));

    if (begin) {
        VkCommandBufferBeginInfo cmdBufferBeginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        check(vkBeginCommandBuffer(handle_, &cmdBufferBeginInfo));
    }
}

void CommandBuffer::submitAndWait()
{
    if (handle_ == VK_NULL_HANDLE) {
        return;
    }

    check(vkEndCommandBuffer(handle_));

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &handle_;

    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = 0;

    VkFence fence;
    check(vkCreateFence(device_, &fenceCreateInfo, nullptr, &fence));

    check(vkQueueSubmit(queue_, 1, &submitInfo, fence));

    check(vkWaitForFences(device_, 1, &fence, VK_TRUE, 1000000000)); // nanoseconds
    vkDestroyFence(device_, fence, nullptr);
}

} // namespace hlab