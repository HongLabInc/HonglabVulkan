#include "engine/Context.h"
#include "engine/Window.h"
#include "engine/Swapchain.h"
#include "engine/CommandBuffer.h"

#include <chrono>
#include <thread>
#include <cmath>

using namespace hlab;
using namespace std;

void transitionImageLayout(VkCommandBuffer cmd, VkImage image, VkPipelineStageFlags2 srcStage,
                           VkPipelineStageFlags2 dstStage, VkAccessFlags2 srcAccess,
                           VkAccessFlags2 dstAccess, VkImageLayout oldLayout,
                           VkImageLayout newLayout)
{
    VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    barrier.srcStageMask = srcStage;
    barrier.dstStageMask = dstStage;
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.image = image;
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
    vkCmdPipelineBarrier2(cmd, &depInfo);
}

VkClearColorValue generateAnimatedColor()
{
    static auto startTime = chrono::high_resolution_clock::now();
    auto currentTime = chrono::high_resolution_clock::now();
    float time = chrono::duration<float>(currentTime - startTime).count();

    float red = (std::sin(time * 0.5f) + 1.0f) * 0.5f;
    float green = (std::sin(time * 0.7f + 1.0f) + 1.0f) * 0.5f;
    float blue = (std::sin(time * 0.9f + 2.0f) + 1.0f) * 0.5f;

    return {{red, green, blue, 1.0f}};
}

void recordCommandBuffer(CommandBuffer& cmd, Swapchain& swapchain, uint32_t imageIndex,
                         VkExtent2D windowSize)
{
    vkResetCommandBuffer(cmd.handle(), 0);
    VkCommandBufferBeginInfo cmdBufferBeginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    check(vkBeginCommandBuffer(cmd.handle(), &cmdBufferBeginInfo));

    transitionImageLayout(cmd.handle(), swapchain.image(imageIndex),
                          VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                          VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_NONE,
                          VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkClearColorValue clearColor = generateAnimatedColor();

    VkRenderingAttachmentInfo colorAttachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    colorAttachment.imageView = swapchain.imageView(imageIndex);
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue.color = clearColor;

    VkRenderingInfo renderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO};
    renderingInfo.renderArea = {0, 0, windowSize.width, windowSize.height};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;

    vkCmdBeginRendering(cmd.handle(), &renderingInfo);
    vkCmdEndRendering(cmd.handle());

    transitionImageLayout(
        cmd.handle(), swapchain.image(imageIndex), VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    check(vkEndCommandBuffer(cmd.handle()));
}

// Keyboard callback function to handle ESC key press
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

int main()
{
    Window window;

    // Set up keyboard callback to close application when ESC is pressed
    window.setKeyCallback(keyCallback);

    VkExtent2D windowSize = window.getFramebufferSize();
    Context ctx(window.getRequiredExtensions(), true);
    Swapchain swapchain(ctx, window.createSurface(ctx.instance()), windowSize, true);

    const uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    vector<CommandBuffer> commandBuffers_ = ctx.createGraphicsCommandBuffers(MAX_FRAMES_IN_FLIGHT);

    vector<VkSemaphore> presentSemaphores_(swapchain.imageCount());
    vector<VkSemaphore> renderSemaphores_(swapchain.imageCount());

    for (size_t i = 0; i < swapchain.imageCount(); i++) {
        VkSemaphoreCreateInfo semaphoreCI{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        check(vkCreateSemaphore(ctx.device(), &semaphoreCI, nullptr, &presentSemaphores_[i]));
        check(vkCreateSemaphore(ctx.device(), &semaphoreCI, nullptr, &renderSemaphores_[i]));
    }

    vector<VkFence> inFlightFences_(MAX_FRAMES_IN_FLIGHT);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkFenceCreateInfo fenceCreateInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        check(vkCreateFence(ctx.device(), &fenceCreateInfo, nullptr, &inFlightFences_[i]));
    }

    uint32_t currentFrame = 0;
    uint32_t currentSemaphore = 0;

    while (!window.isCloseRequested()) {
        window.pollEvents();

        check(
            vkWaitForFences(ctx.device(), 1, &inFlightFences_[currentFrame], VK_TRUE, UINT64_MAX));
        check(vkResetFences(ctx.device(), 1, &inFlightFences_[currentFrame]));

        uint32_t imageIndex = 0;
        VkResult result =
            swapchain.acquireNextImage(presentSemaphores_[currentSemaphore], imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            continue;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            exitWithMessage("Failed to acquire swapchain image!");
        }

        recordCommandBuffer(commandBuffers_[currentFrame], swapchain, imageIndex, windowSize);

        // Create semaphore submit infos
        VkSemaphoreSubmitInfo waitSemaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
        waitSemaphoreInfo.semaphore = presentSemaphores_[currentSemaphore];
        waitSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        waitSemaphoreInfo.value = 0; // Binary semaphore
        waitSemaphoreInfo.deviceIndex = 0;

        VkSemaphoreSubmitInfo signalSemaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
        signalSemaphoreInfo.semaphore = renderSemaphores_[currentSemaphore];
        signalSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        signalSemaphoreInfo.value = 0; // Binary semaphore
        signalSemaphoreInfo.deviceIndex = 0;

        VkCommandBufferSubmitInfo cmdBufferInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
        cmdBufferInfo.commandBuffer = commandBuffers_[currentFrame].handle();
        cmdBufferInfo.deviceMask = 0;

        VkSubmitInfo2 submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
        submitInfo.waitSemaphoreInfoCount = 1;
        submitInfo.pWaitSemaphoreInfos = &waitSemaphoreInfo;
        submitInfo.commandBufferInfoCount = 1;
        submitInfo.pCommandBufferInfos = &cmdBufferInfo;
        submitInfo.signalSemaphoreInfoCount = 1;
        submitInfo.pSignalSemaphoreInfos = &signalSemaphoreInfo;

        check(vkQueueSubmit2(commandBuffers_[currentFrame].queue(), 1, &submitInfo,
                             inFlightFences_[currentFrame]));

        VkResult presentResult = swapchain.queuePresent(ctx.graphicsQueue(), imageIndex,
                                                        renderSemaphores_[currentSemaphore]);

        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {

        } else if (presentResult != VK_SUCCESS) {
            exitWithMessage("Failed to present swapchain image!");
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        currentSemaphore = (currentSemaphore + 1) % swapchain.imageCount();
    }

    ctx.waitIdle();

    for (auto& semaphore : presentSemaphores_) {
        vkDestroySemaphore(ctx.device(), semaphore, nullptr);
    }
    for (auto& semaphore : renderSemaphores_) {
        vkDestroySemaphore(ctx.device(), semaphore, nullptr);
    }
    for (auto& fence : inFlightFences_) {
        vkDestroyFence(ctx.device(), fence, nullptr);
    }

    return 0;
}
