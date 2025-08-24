#include "engine/Context.h"
#include "engine/Window.h"
#include "engine/Swapchain.h"
#include "engine/CommandBuffer.h"

#include <chrono>
#include <thread>

using namespace hlab;
using namespace std;

int main()
{
    Window window;
    VkExtent2D windowSize = window.getFramebufferSize();

    Context ctx(window.getRequiredExtensions(), true);

    Swapchain swapchain(ctx, window.createSurface(ctx.instance()), windowSize);

    const uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    vector<VkSemaphore> presentCompleteSemaphores_{};
    vector<VkSemaphore> renderCompleteSemaphores_{};
    vector<VkFence> inFlightFences_{};
    vector<CommandBuffer> commandBuffers_{};

    presentCompleteSemaphores_.resize(swapchain.imageCount());
    renderCompleteSemaphores_.resize(swapchain.imageCount());

    for (size_t i = 0; i < swapchain.imageCount(); i++) {
        VkSemaphoreCreateInfo semaphoreCI{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        check(
            vkCreateSemaphore(ctx.device(), &semaphoreCI, nullptr, &presentCompleteSemaphores_[i]));
        check(
            vkCreateSemaphore(ctx.device(), &semaphoreCI, nullptr, &renderCompleteSemaphores_[i]));
    }

    inFlightFences_.resize(MAX_FRAMES_IN_FLIGHT);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkFenceCreateInfo fenceCreateInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        check(vkCreateFence(ctx.device(), &fenceCreateInfo, nullptr, &inFlightFences_[i]));
    }

    commandBuffers_ = ctx.createGraphicsCommandBuffers(MAX_FRAMES_IN_FLIGHT);

    uint32_t currentFrame = 0;
    uint32_t currentSemaphore = 0;

    while (!window.isCloseRequested()) {
        window.pollEvents();

        check(
            vkWaitForFences(ctx.device(), 1, &inFlightFences_[currentFrame], VK_TRUE, UINT64_MAX));
        check(vkResetFences(ctx.device(), 1, &inFlightFences_[currentFrame]));

        uint32_t imageIndex = 0;
        VkResult result =
            swapchain.acquireNextImage(presentCompleteSemaphores_[currentSemaphore], imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            continue;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            exitWithMessage("Failed to acquire swapchain image!");
        }

        static auto startTime = chrono::high_resolution_clock::now();
        auto currentTime = chrono::high_resolution_clock::now();
        float time = chrono::duration<float>(currentTime - startTime).count();

        float red = (sin(time * 0.5f) + 1.0f) * 0.5f;
        float green = (sin(time * 0.7f + 1.0f) + 1.0f) * 0.5f;
        float blue = (sin(time * 0.9f + 2.0f) + 1.0f) * 0.5f;

        VkClearColorValue clearColor = {{red, green, blue, 1.0f}};

        CommandBuffer& cmd = commandBuffers_[currentFrame];

        vkResetCommandBuffer(cmd.handle(), 0);
        VkCommandBufferBeginInfo cmdBufferBeginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        check(vkBeginCommandBuffer(cmd.handle(), &cmdBufferBeginInfo));

        {
            VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

            barrier.srcAccessMask = VK_ACCESS_2_NONE;
            barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            barrier.image = swapchain.image(imageIndex);
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;

            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

            VkDependencyInfo depInfo{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
            depInfo.imageMemoryBarrierCount = 1;
            depInfo.pImageMemoryBarriers = &barrier;
            vkCmdPipelineBarrier2(cmd.handle(), &depInfo);
        }

        VkRenderingAttachmentInfo colorAttachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        colorAttachment.imageView = swapchain.imageView(imageIndex);
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue.color = clearColor;

        VkRenderingInfo renderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO_KHR};
        renderingInfo.renderArea = {0, 0, windowSize.width, windowSize.height};
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &colorAttachment;

        vkCmdBeginRendering(cmd.handle(), &renderingInfo);
        vkCmdEndRendering(cmd.handle());

        {
            VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;

            barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_2_NONE;

            barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

            barrier.image = swapchain.image(imageIndex);
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

            VkDependencyInfo depInfo{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
            depInfo.imageMemoryBarrierCount = 1;
            depInfo.pImageMemoryBarriers = &barrier;
            vkCmdPipelineBarrier2(cmd.handle(), &depInfo);
        }

        check(vkEndCommandBuffer(cmd.handle()));

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &presentCompleteSemaphores_[currentSemaphore];
        submitInfo.pWaitDstStageMask = &waitStage;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd.handle();

        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &renderCompleteSemaphores_[currentSemaphore];

        check(vkQueueSubmit(cmd.queue(), 1, &submitInfo, inFlightFences_[currentFrame]));

        VkResult presentResult = swapchain.queuePresent(
            ctx.graphicsQueue(), imageIndex, renderCompleteSemaphores_[currentSemaphore]);

        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {

        } else if (presentResult != VK_SUCCESS) {
            exitWithMessage("Failed to present swapchain image!");
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        currentSemaphore = (currentSemaphore + 1) % swapchain.imageCount();
    }

    ctx.waitIdle();

    for (auto& semaphore : presentCompleteSemaphores_) {
        vkDestroySemaphore(ctx.device(), semaphore, nullptr);
    }
    for (auto& semaphore : renderCompleteSemaphores_) {
        vkDestroySemaphore(ctx.device(), semaphore, nullptr);
    }
    for (auto& fence : inFlightFences_) {
        vkDestroyFence(ctx.device(), fence, nullptr);
    }

    return 0;
}