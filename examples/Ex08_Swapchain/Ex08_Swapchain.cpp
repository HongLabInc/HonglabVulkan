#include "engine/Context.h"
#include "engine/Window.h"
#include "engine/Swapchain.h"

#include <chrono>
#include <thread>

using namespace hlab;
using namespace std;

int main()
{
    // Initialize window and get its framebuffer size
    Window window;
    VkExtent2D windowSize = window.getFramebufferSize();

    // Create Vulkan context with required extensions and swapchain support
    Context ctx(window.getRequiredExtensions(), true);

    // Create swapchain - a series of images that can be presented to the screen
    // The swapchain manages multiple images to enable smooth rendering without tearing
    Swapchain swapchain(ctx, window.createSurface(ctx.instance()), windowSize);

    // FRAMES IN FLIGHT: This is a key Vulkan concept for performance
    // We use 2 frames in flight to allow the CPU to work on the next frame
    // while the GPU is still processing the current frame
    const uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    // SYNCHRONIZATION OBJECTS: Vulkan requires explicit synchronization
    vector<VkSemaphore>
        presentCompleteSemaphores_{}; // GPU-GPU sync: signals when image acquisition completes
    vector<VkSemaphore>
        renderCompleteSemaphores_{};         // GPU-GPU sync: signals when rendering completes
    vector<VkFence> inFlightFences_{};       // CPU-GPU sync: prevents CPU from getting ahead of GPU
    vector<CommandBuffer> commandBuffers_{}; // Command buffers record GPU commands

    // SEMAPHORE ALLOCATION STRATEGY:
    // We create one semaphore pair for each swapchain image to avoid conflicts
    // This ensures that each image has its own synchronization objects
    presentCompleteSemaphores_.resize(swapchain.imageCount());
    renderCompleteSemaphores_.resize(swapchain.imageCount());

    for (size_t i = 0; i < swapchain.imageCount(); i++) {
        VkSemaphoreCreateInfo semaphoreCI{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        // Present complete semaphore: signaled when vkAcquireNextImageKHR completes
        check(
            vkCreateSemaphore(ctx.device(), &semaphoreCI, nullptr, &presentCompleteSemaphores_[i]));
        // Render complete semaphore: signaled when our rendering commands complete
        check(
            vkCreateSemaphore(ctx.device(), &semaphoreCI, nullptr, &renderCompleteSemaphores_[i]));
    }

    // FENCE ALLOCATION STRATEGY:
    // We create fences for frames in flight (usually fewer than swapchain images)
    // Fences allow the CPU to wait for GPU work to complete
    inFlightFences_.resize(MAX_FRAMES_IN_FLIGHT);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkFenceCreateInfo fenceCreateInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        // IMPORTANT: Start fences in signaled state so the first frame doesn't hang
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        check(vkCreateFence(ctx.device(), &fenceCreateInfo, nullptr, &inFlightFences_[i]));
    }

    // COMMAND BUFFER ALLOCATION:
    // Create command buffers - one for each frame in flight
    // Command buffers record sequences of GPU commands to be executed
    commandBuffers_ = ctx.createGraphicsCommandBuffers(MAX_FRAMES_IN_FLIGHT);

    // FRAME TRACKING INDICES:
    uint32_t currentFrame = 0;     // Tracks CPU resources (command buffers, fences)
    uint32_t currentSemaphore = 0; // Tracks GPU semaphores (cycles through swapchain images)

    // MAIN RENDER LOOP
    while (!window.isCloseRequested()) {
        window.pollEvents();

        // STEP 1: CPU-GPU SYNCHRONIZATION
        // Wait for the fence of the current frame to be signaled
        // This prevents the CPU from getting more than MAX_FRAMES_IN_FLIGHT ahead of the GPU
        check(
            vkWaitForFences(ctx.device(), 1, &inFlightFences_[currentFrame], VK_TRUE, UINT64_MAX));
        check(vkResetFences(ctx.device(), 1, &inFlightFences_[currentFrame]));

        // STEP 2: ACQUIRE SWAPCHAIN IMAGE
        // Get the next available image from the swapchain for rendering
        // This operation is asynchronous and signals presentCompleteSemaphore when done
        uint32_t imageIndex = 0;
        VkResult result =
            swapchain.acquireNextImage(presentCompleteSemaphores_[currentSemaphore], imageIndex);

        // Handle swapchain out-of-date scenarios (window resize, etc.)
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            continue; // Skip this frame and try again
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            exitWithMessage("Failed to acquire swapchain image!");
        }

        // STEP 3: GENERATE ANIMATED CONTENT
        // Create a time-based color animation using sine waves
        static auto startTime = chrono::high_resolution_clock::now();
        auto currentTime = chrono::high_resolution_clock::now();
        float time = chrono::duration<float>(currentTime - startTime).count();

        // Generate smooth color transitions using trigonometric functions
        float red = (sin(time * 0.5f) + 1.0f) * 0.5f;          // Slow red oscillation
        float green = (sin(time * 0.7f + 1.0f) + 1.0f) * 0.5f; // Medium green with phase offset
        float blue = (sin(time * 0.9f + 2.0f) + 1.0f) * 0.5f;  // Fast blue with different phase

        VkClearColorValue clearColor = {{red, green, blue, 1.0f}};

        // STEP 4: RECORD COMMAND BUFFER
        // Command buffers must be recorded every frame as they contain frame-specific data
        CommandBuffer& cmd = commandBuffers_[currentFrame];

        // Reset the command buffer to start recording new commands
        vkResetCommandBuffer(cmd.handle(), 0);
        VkCommandBufferBeginInfo cmdBufferBeginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        check(vkBeginCommandBuffer(cmd.handle(), &cmdBufferBeginInfo));

        // STEP 5: IMAGE LAYOUT TRANSITION (UNDEFINED → COLOR_ATTACHMENT_OPTIMAL)
        // Vulkan requires explicit image layout transitions for optimal performance
        // This barrier transitions the swapchain image to a layout suitable for rendering
        {
            VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
            // Pipeline stages: when to execute this barrier
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT; // Start of pipeline
            barrier.dstStageMask =
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT; // Color output stage

            // Memory access patterns: what operations are allowed before/after
            barrier.srcAccessMask = VK_ACCESS_2_NONE;                       // No prior access
            barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT; // Allow color writes

            // Layout transition: optimize memory layout for the intended usage
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Don't care about current content
            barrier.newLayout =
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // Optimize for color rendering

            // Target image and subresource specification
            barrier.image = swapchain.image(imageIndex);
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // Color data only
            barrier.subresourceRange.baseMipLevel = 0;                       // Base mip level
            barrier.subresourceRange.levelCount = 1;                         // Single mip level
            barrier.subresourceRange.baseArrayLayer = 0;                     // Base array layer
            barrier.subresourceRange.layerCount = 1;                         // Single layer

            // Queue family ownership (not transferring between queues)
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

            // Execute the barrier using modern Vulkan 1.3 synchronization
            VkDependencyInfo depInfo{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
            depInfo.imageMemoryBarrierCount = 1;
            depInfo.pImageMemoryBarriers = &barrier;
            vkCmdPipelineBarrier2(cmd.handle(), &depInfo);
        }

        // STEP 6: RENDERING USING DYNAMIC RENDERING (VULKAN 1.3)
        // Dynamic rendering allows us to render without creating render passes
        VkRenderingAttachmentInfo colorAttachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        colorAttachment.imageView = swapchain.imageView(imageIndex);            // Target image view
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // Expected layout
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;                   // Clear on load
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;                 // Store result
        colorAttachment.clearValue.color = clearColor;                          // Clear color value

        // Configure the rendering area and layers
        VkRenderingInfo renderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO_KHR};
        renderingInfo.renderArea = {0, 0, windowSize.width, windowSize.height}; // Full window
        renderingInfo.layerCount = 1;                                           // Single layer
        renderingInfo.colorAttachmentCount = 1;                                 // One color target
        renderingInfo.pColorAttachments = &colorAttachment;

        // Begin rendering (this will clear the image with our animated color)
        vkCmdBeginRendering(cmd.handle(), &renderingInfo);
        // No draw commands - we're just clearing the screen
        vkCmdEndRendering(cmd.handle());

        // STEP 7: IMAGE LAYOUT TRANSITION (COLOR_ATTACHMENT_OPTIMAL → PRESENT_SRC)
        // Transition the image to a layout suitable for presentation
        {
            VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
            // Pipeline stages: after color output, before present
            barrier.srcStageMask =
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;           // After rendering
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT; // End of pipeline

            // Memory access: from color writes to no specific access (present engine handles it)
            barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT; // Color writes done
            barrier.dstAccessMask = VK_ACCESS_2_NONE; // Present handles access

            // Layout transition: optimize for presentation
            barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // Current layout
            barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;          // Present-ready layout

            // Same image and subresource specification as before
            barrier.image = swapchain.image(imageIndex);
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

            // Execute the barrier
            VkDependencyInfo depInfo{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
            depInfo.imageMemoryBarrierCount = 1;
            depInfo.pImageMemoryBarriers = &barrier;
            vkCmdPipelineBarrier2(cmd.handle(), &depInfo);
        }

        // Finish recording the command buffer
        check(vkEndCommandBuffer(cmd.handle()));

        // STEP 8: SUBMIT COMMAND BUFFER TO GPU
        // Configure synchronization for command buffer submission
        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        // Wait for image acquisition to complete before starting color output
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &presentCompleteSemaphores_[currentSemaphore];
        submitInfo.pWaitDstStageMask = &waitStage;

        // The command buffer to execute
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd.handle();

        // Signal when rendering is complete
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &renderCompleteSemaphores_[currentSemaphore];

        // Submit to GPU queue with fence for CPU synchronization
        check(vkQueueSubmit(cmd.queue(), 1, &submitInfo, inFlightFences_[currentFrame]));

        // STEP 9: PRESENT THE RENDERED IMAGE
        // Present the image to the screen, waiting for rendering to complete
        VkResult presentResult = swapchain.queuePresent(
            ctx.graphicsQueue(), imageIndex, renderCompleteSemaphores_[currentSemaphore]);

        // Handle presentation results
        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
            // Swapchain needs recreation (window resize, etc.)
            // In a full application, you would recreate the swapchain here
        } else if (presentResult != VK_SUCCESS) {
            exitWithMessage("Failed to present swapchain image!");
        }

        // STEP 10: ADVANCE TO NEXT FRAME
        // Update frame indices for the next iteration
        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;           // CPU resources cycle
        currentSemaphore = (currentSemaphore + 1) % swapchain.imageCount(); // GPU semaphores cycle
    }

    // CLEANUP: Wait for all GPU operations to complete before destroying resources
    ctx.waitIdle();

    // Command buffers are automatically cleaned up by their destructors

    // Cleanup synchronization objects
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
