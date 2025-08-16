#pragma once

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

#include "BarrierHelper.h"
#include "Context.h"
#include "Logger.h"

namespace hlab {

using namespace std;

class Swapchain
{
  public:
    Swapchain(Context& ctx, VkSurfaceKHR surface, VkExtent2D& windowSize);
    ~Swapchain()
    {
        cleanup();
    }

    void initSurface(VkSurfaceKHR surface);
    void create(VkExtent2D& exectedWindowSize, bool vsync = false, bool fullscreen = false);
    void cleanup();

    auto acquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t& imageIndex) -> VkResult;
    auto queuePresent(VkQueue queue, uint32_t imageIndex,
                      VkSemaphore waitSemaphore = VK_NULL_HANDLE) -> VkResult;

    auto colorFormat() -> VkFormat
    {
        return colorFormat_;
    }

    auto images() -> vector<VkImage>&
    {
        return images_;
    }

    auto image(uint32_t imageIndex) -> VkImage&
    {
        return images_[imageIndex];
    }

    auto imageCount() const -> uint32_t
    {
        return imageCount_;
    }

    auto handle() -> VkSwapchainKHR&
    {
        return swapchain_;
    }

    auto imageViews() -> vector<VkImageView>&
    {
        return imageViews_;
    }

    auto imageView(uint32_t index) -> VkImageView
    {
        return imageViews_[index];
    }

    auto barrierHelper(uint32_t index) -> BarrierHelper&
    {
        return barrierHelpers[index];
    }

  private:
    Context& ctx_;
    VkSurfaceKHR surface_{VK_NULL_HANDLE};

    VkFormat colorFormat_{};
    VkColorSpaceKHR colorSpace_{}; // Linear vs Non-Linear
    VkSwapchainKHR swapchain_{VK_NULL_HANDLE};
    vector<VkImage> images_{};
    vector<VkImageView> imageViews_{};
    uint32_t imageCount_{0};

    vector<BarrierHelper> barrierHelpers{};
};

} // namespace hlab