#pragma once

#include <vulkan/vulkan.h>

namespace hlab {

class CommandBuffer
{
  public:
    CommandBuffer(VkDevice& device, VkCommandBuffer& handle, VkCommandPool& commandPool,
                  VkQueue& queue, VkCommandBufferLevel level);
    CommandBuffer(VkDevice& device, VkCommandPool& commandPool, VkQueue& queue,
                  VkCommandBufferLevel level, bool begin);

    // Move constructor
    CommandBuffer(CommandBuffer&& other) noexcept
        : device_(other.device_), commandPool_(other.commandPool_), queue_(other.queue_),
          handle_(other.handle_)
    {
        other.handle_ = VK_NULL_HANDLE;
    }

    ~CommandBuffer()
    {
        cleanup();
    }

    void cleanup()
    {
        if (handle_ != VK_NULL_HANDLE) {
            vkFreeCommandBuffers(device_, commandPool_, 1, &handle_);
            handle_ = VK_NULL_HANDLE;
        }
    }

    void submitAndWait();

    auto handle() -> VkCommandBuffer&
    {
        return handle_;
    }

    auto queue() const -> VkQueue
    {
        return queue_;
    }

  private:
    VkDevice& device_;
    VkCommandPool& commandPool_;
    VkQueue& queue_;

    VkCommandBuffer handle_{VK_NULL_HANDLE};
};

} // namespace hlab
