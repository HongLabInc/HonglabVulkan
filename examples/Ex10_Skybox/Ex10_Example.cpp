#include "Ex10_Example.h"
#include "engine/Logger.h"

#include <chrono>
#include <thread>
#include <filesystem>
#include <stdexcept>

using namespace hlab;

Ex10_Example::Ex10_Example()
    : window_{}, ctx_{window_.getRequiredExtensions(), true},
      windowSize_{window_.getFramebufferSize()},
      swapchain_{ctx_, window_.createSurface(ctx_.instance()), windowSize_, true},
      shaderManager_{ctx_, kShaderPathPrefix, {{"gui", {"imgui.vert", "imgui.frag"}}}},
      guiRenderer_{ctx_, shaderManager_, swapchain_.colorFormat()}
{
    initialize();
}

Ex10_Example::~Ex10_Example()
{
    cleanup();
}

void Ex10_Example::run()
{
    printLog("Current working directory: {}", std::filesystem::current_path().string());

    // Initialize GUI
    guiRenderer_.resize(windowSize_.width, windowSize_.height);

    mainLoop();
}

void Ex10_Example::initialize()
{
    // Set up GLFW callbacks
    window_.setKeyCallback(keyCallback);
    window_.setMouseButtonCallback(mouseButtonCallback);
    window_.setCursorPosCallback(cursorPosCallback);

    // Store this instance in GLFW user pointer for callback access
    window_.setUserPointer(this);

    // Setup frame resources
    commandBuffers_ = ctx_.createGraphicsCommandBuffers(kMaxFramesInFlight);

    initializeSynchronization();
}

void Ex10_Example::cleanup()
{
    ctx_.waitIdle();
    cleanupSynchronization();
}

void Ex10_Example::mainLoop()
{
    while (!window_.isCloseRequested() && !shouldClose_) {
        window_.pollEvents();
        renderFrame();
    }
}

void Ex10_Example::renderFrame()
{
    updateGui(windowSize_);
    guiRenderer_.update();

    check(vkWaitForFences(ctx_.device(), 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX));
    check(vkResetFences(ctx_.device(), 1, &inFlightFences_[currentFrame_]));

    uint32_t imageIndex = 0;
    VkResult acquireResult =
        swapchain_.acquireNextImage(presentSemaphores_[currentSemaphore_], imageIndex);

    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        exitWithMessage("Window resize not implemented");
    } else if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
        exitWithMessage("Failed to acquire swapchain image!");
    }

    recordCommandBuffer(commandBuffers_[currentFrame_], imageIndex, windowSize_);

    submitFrame(commandBuffers_[currentFrame_], presentSemaphores_[currentSemaphore_],
                renderSemaphores_[currentSemaphore_], inFlightFences_[currentFrame_]);

    // Present frame
    VkResult presentResult = swapchain_.queuePresent(ctx_.graphicsQueue(), imageIndex,
                                                     renderSemaphores_[currentSemaphore_]);

    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        exitWithMessage("Window resize not implemented");
    } else if (presentResult != VK_SUCCESS) {
        exitWithMessage("Failed to present swapchain image!");
    }

    currentFrame_ = (currentFrame_ + 1) % kMaxFramesInFlight;
    currentSemaphore_ = (currentSemaphore_ + 1) % swapchain_.imageCount();
}

void Ex10_Example::updateGui(VkExtent2D windowSize)
{
    ImGuiIO& io = ImGui::GetIO();

    // Update ImGui IO state
    io.DisplaySize = ImVec2(float(windowSize.width), float(windowSize.height));
    io.MousePos = ImVec2(mouseState_.position.x, mouseState_.position.y);
    io.MouseDown[0] = mouseState_.buttons.left;
    io.MouseDown[1] = mouseState_.buttons.right;
    io.MouseDown[2] = mouseState_.buttons.middle;

    // Begin GUI frame
    ImGui::NewFrame();

    // Render color control window
    renderColorControlWindow();

    ImGui::Render();
}

void Ex10_Example::renderColorControlWindow()
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Clear Color Control")) {
        ImGui::End();
        return;
    }

    ImGui::Text("Control the background clear color:");
    ImGui::Separator();

    // Color picker (using ColorEdit4 to include alpha channel)
    ImGui::ColorEdit4("Clear Color", &clearColor_.r);

    ImGui::Separator();
    ImGui::Text("Individual Controls:");

    // RGBA sliders
    ImGui::SliderFloat("Red", &clearColor_.r, 0.0f, 1.0f, "%.3f");
    ImGui::SliderFloat("Green", &clearColor_.g, 0.0f, 1.0f, "%.3f");
    ImGui::SliderFloat("Blue", &clearColor_.b, 0.0f, 1.0f, "%.3f");
    ImGui::SliderFloat("Alpha", &clearColor_.a, 0.0f, 1.0f, "%.3f");

    ImGui::Separator();
    ImGui::Text("Color Preview:");

    // Preview button
    ImGui::ColorButton("Preview",
                       ImVec4(clearColor_.r, clearColor_.g, clearColor_.b, clearColor_.a), 0,
                       ImVec2(50, 50));

    ImGui::Separator();
    ImGui::Text("Presets:");

    // Preset buttons with better layout
    renderColorPresets();

    ImGui::End();
}

void Ex10_Example::renderColorPresets()
{
    const struct ColorPreset
    {
        const char* name;
        glm::vec4 color;
    } presets[] = {{"Sky Blue", {0.53f, 0.81f, 0.92f, 1.0f}},
                   {"Sunset", {1.0f, 0.65f, 0.0f, 1.0f}},
                   {"Night", {0.05f, 0.05f, 0.15f, 1.0f}},
                   {"Forest", {0.13f, 0.55f, 0.13f, 1.0f}},
                   {"Reset", {0.2f, 0.3f, 0.5f, 1.0f}}};

    constexpr int buttonsPerRow = 2;
    for (int i = 0; i < std::size(presets); ++i) {
        if (ImGui::Button(presets[i].name)) {
            clearColor_ = presets[i].color;
        }

        if ((i + 1) % buttonsPerRow != 0 && i < std::size(presets) - 1) {
            ImGui::SameLine();
        }
    }
}

void Ex10_Example::recordCommandBuffer(CommandBuffer& cmd, uint32_t imageIndex,
                                       VkExtent2D windowSize)
{
    vkResetCommandBuffer(cmd.handle(), 0);
    VkCommandBufferBeginInfo cmdBufferBeginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    check(vkBeginCommandBuffer(cmd.handle(), &cmdBufferBeginInfo));

    swapchain_.barrierHelper(imageIndex)
        .transitionTo(cmd.handle(), VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

    // Use clearColor directly
    VkClearColorValue clearColorValue = {
        {clearColor_.r, clearColor_.g, clearColor_.b, clearColor_.a}};

    VkRenderingAttachmentInfo colorAttachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    colorAttachment.imageView = swapchain_.imageView(imageIndex);
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue.color = clearColorValue;

    VkRenderingInfo renderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO};
    renderingInfo.renderArea = {0, 0, windowSize.width, windowSize.height};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;

    vkCmdBeginRendering(cmd.handle(), &renderingInfo);
    vkCmdEndRendering(cmd.handle());

    // Draw GUI on top of the clear color
    VkViewport viewport{0.0f, 0.0f, (float)windowSize.width, (float)windowSize.height, 0.0f, 1.0f};
    guiRenderer_.draw(cmd.handle(), swapchain_.imageView(imageIndex), viewport);

    swapchain_.barrierHelper(imageIndex)
        .transitionTo(cmd.handle(), VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                      VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);

    check(vkEndCommandBuffer(cmd.handle()));
}

void Ex10_Example::submitFrame(CommandBuffer& commandBuffer, VkSemaphore waitSemaphore,
                               VkSemaphore signalSemaphore, VkFence fence)
{
    VkSemaphoreSubmitInfo waitSemaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
    waitSemaphoreInfo.semaphore = waitSemaphore;
    waitSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    waitSemaphoreInfo.value = 0;
    waitSemaphoreInfo.deviceIndex = 0;

    VkSemaphoreSubmitInfo signalSemaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
    signalSemaphoreInfo.semaphore = signalSemaphore;
    signalSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    signalSemaphoreInfo.value = 0;
    signalSemaphoreInfo.deviceIndex = 0;

    VkCommandBufferSubmitInfo cmdBufferInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
    cmdBufferInfo.commandBuffer = commandBuffer.handle();
    cmdBufferInfo.deviceMask = 0;

    VkSubmitInfo2 submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
    submitInfo.waitSemaphoreInfoCount = 1;
    submitInfo.pWaitSemaphoreInfos = &waitSemaphoreInfo;
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &cmdBufferInfo;
    submitInfo.signalSemaphoreInfoCount = 1;
    submitInfo.pSignalSemaphoreInfos = &signalSemaphoreInfo;

    check(vkQueueSubmit2(commandBuffer.queue(), 1, &submitInfo, fence));
}

void Ex10_Example::initializeSynchronization()
{
    uint32_t imageCount = swapchain_.imageCount();

    // Create semaphores
    presentSemaphores_.resize(imageCount);
    renderSemaphores_.resize(imageCount);
    for (size_t i = 0; i < imageCount; i++) {
        VkSemaphoreCreateInfo semaphoreCI{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        check(vkCreateSemaphore(ctx_.device(), &semaphoreCI, nullptr, &presentSemaphores_[i]));
        check(vkCreateSemaphore(ctx_.device(), &semaphoreCI, nullptr, &renderSemaphores_[i]));
    }

    // Create fences
    inFlightFences_.resize(kMaxFramesInFlight);
    for (size_t i = 0; i < kMaxFramesInFlight; i++) {
        VkFenceCreateInfo fenceCreateInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        check(vkCreateFence(ctx_.device(), &fenceCreateInfo, nullptr, &inFlightFences_[i]));
    }
}

void Ex10_Example::cleanupSynchronization()
{
    for (auto& semaphore : presentSemaphores_) {
        vkDestroySemaphore(ctx_.device(), semaphore, nullptr);
    }
    for (auto& semaphore : renderSemaphores_) {
        vkDestroySemaphore(ctx_.device(), semaphore, nullptr);
    }
    for (auto& fence : inFlightFences_) {
        vkDestroyFence(ctx_.device(), fence, nullptr);
    }
}

// Static callback implementations
void Ex10_Example::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    Ex10_Example* example = static_cast<Ex10_Example*>(glfwGetWindowUserPointer(window));
    if (example) {
        example->handleKeyInput(key, scancode, action, mods);
    }
}

void Ex10_Example::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    Ex10_Example* example = static_cast<Ex10_Example*>(glfwGetWindowUserPointer(window));
    if (example) {
        example->handleMouseButton(button, action, mods);
    }
}

void Ex10_Example::cursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{
    Ex10_Example* example = static_cast<Ex10_Example*>(glfwGetWindowUserPointer(window));
    if (example) {
        example->handleCursorPos(xpos, ypos);
    }
}

// Instance callback handlers
void Ex10_Example::handleKeyInput(int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        // Set a flag to close the window - the main loop will check this
        shouldClose_ = true;
    }
}

void Ex10_Example::handleMouseButton(int button, int action, int mods)
{
    if (action == GLFW_PRESS) {
        switch (button) {
        case GLFW_MOUSE_BUTTON_LEFT:
            mouseState_.buttons.left = true;
            break;
        case GLFW_MOUSE_BUTTON_RIGHT:
            mouseState_.buttons.right = true;
            break;
        case GLFW_MOUSE_BUTTON_MIDDLE:
            mouseState_.buttons.middle = true;
            break;
        }
    } else if (action == GLFW_RELEASE) {
        switch (button) {
        case GLFW_MOUSE_BUTTON_LEFT:
            mouseState_.buttons.left = false;
            break;
        case GLFW_MOUSE_BUTTON_RIGHT:
            mouseState_.buttons.right = false;
            break;
        case GLFW_MOUSE_BUTTON_MIDDLE:
            mouseState_.buttons.middle = false;
            break;
        }
    }
}

void Ex10_Example::handleCursorPos(double xpos, double ypos)
{
    mouseState_.position = glm::vec2(static_cast<float>(xpos), static_cast<float>(ypos));
}