#include "engine/Context.h"
#include "engine/Window.h"
#include "engine/Swapchain.h"
#include "engine/CommandBuffer.h"
#include "engine/GuiRenderer.h"
#include "engine/ShaderManager.h"

#include <chrono>
#include <thread>
#include <glm/glm.hpp>

using namespace hlab;
using namespace std;

// Mouse state structure (similar to Application class)
struct MouseState
{
    struct
    {
        bool left = false;
        bool right = false;
        bool middle = false;
    } buttons;
    glm::vec2 position{0.0f, 0.0f};
};

// Global variables
glm::vec3 clearColor = glm::vec3(0.2f, 0.3f, 0.5f); // Default blue-ish color
MouseState mouseState;

VkClearColorValue generateAnimatedColor()
{
    static auto startTime = chrono::high_resolution_clock::now();
    auto currentTime = chrono::high_resolution_clock::now();
    float time = chrono::duration<float>(currentTime - startTime).count();

    float red = (sin(time * 0.5f) + 1.0f) * 0.5f;
    float green = (sin(time * 0.7f + 1.0f) + 1.0f) * 0.5f;
    float blue = (sin(time * 0.9f + 2.0f) + 1.0f) * 0.5f;

    return {{red, green, blue, 1.0f}};
}

VkClearColorValue getGuiControlledColor()
{
    return {{clearColor.r, clearColor.g, clearColor.b, 1.0f}};
}

void updateGui(VkExtent2D windowSize)
{
    ImGuiIO& io = ImGui::GetIO();

    // Set display size and mouse input for ImGui
    io.DisplaySize = ImVec2(float(windowSize.width), float(windowSize.height));

    // Pass mouse input to ImGui - let ImGui decide if it wants to capture it
    io.MousePos = ImVec2(mouseState.position.x, mouseState.position.y);
    io.MouseDown[0] = mouseState.buttons.left;   // Left mouse button
    io.MouseDown[1] = mouseState.buttons.right;  // Right mouse button
    io.MouseDown[2] = mouseState.buttons.middle; // Middle mouse button

    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver);
    ImGui::Begin("Clear Color Control");

    ImGui::Text("Control the background clear color:");
    ImGui::Separator();

    // Color picker for clear color
    ImGui::ColorEdit3("Clear Color", &clearColor.r);

    ImGui::Separator();
    ImGui::Text("Individual Controls:");

    // Individual RGB sliders for more precise control
    ImGui::SliderFloat("Red", &clearColor.r, 0.0f, 1.0f, "%.3f");
    ImGui::SliderFloat("Green", &clearColor.g, 0.0f, 1.0f, "%.3f");
    ImGui::SliderFloat("Blue", &clearColor.b, 0.0f, 1.0f, "%.3f");

    ImGui::Separator();
    ImGui::Text("Color Preview:");

    // Color preview
    ImGui::ColorButton("Preview", ImVec4(clearColor.r, clearColor.g, clearColor.b, 1.0f),
                       ImGuiColorEditFlags_NoAlpha, ImVec2(50, 50));

    ImGui::Separator();
    ImGui::Text("Presets:");

    // Preset colors
    if (ImGui::Button("Sky Blue")) {
        clearColor = glm::vec3(0.53f, 0.81f, 0.92f);
    }
    ImGui::SameLine();
    if (ImGui::Button("Sunset")) {
        clearColor = glm::vec3(1.0f, 0.65f, 0.0f);
    }

    if (ImGui::Button("Night")) {
        clearColor = glm::vec3(0.05f, 0.05f, 0.15f);
    }
    ImGui::SameLine();
    if (ImGui::Button("Forest")) {
        clearColor = glm::vec3(0.13f, 0.55f, 0.13f);
    }

    if (ImGui::Button("Reset")) {
        clearColor = glm::vec3(0.2f, 0.3f, 0.5f);
    }

    ImGui::End();
    ImGui::Render();
}

void handleMouseMove(int32_t x, int32_t y)
{
    // Update mouse position
    mouseState.position = glm::vec2((float)x, (float)y);

    // Note: We don't need to handle camera movement here since this is just a GUI example
    // In a full application, you might check ImGui::GetIO().WantCaptureMouse here
}

void recordCommandBuffer(CommandBuffer& cmd, Swapchain& swapchain, uint32_t imageIndex,
                         VkExtent2D windowSize, GuiRenderer& guiRenderer)
{
    vkResetCommandBuffer(cmd.handle(), 0);
    VkCommandBufferBeginInfo cmdBufferBeginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    check(vkBeginCommandBuffer(cmd.handle(), &cmdBufferBeginInfo));

    swapchain.barrierHelper(imageIndex)
        .transitionTo(cmd.handle(), VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

    // Use GUI-controlled clear color instead of animated color
    VkClearColorValue clearColorValue = getGuiControlledColor();

    VkRenderingAttachmentInfo colorAttachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    colorAttachment.imageView = swapchain.imageView(imageIndex);
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
    guiRenderer.draw(cmd.handle(), swapchain.imageView(imageIndex), viewport);

    swapchain.barrierHelper(imageIndex)
        .transitionTo(cmd.handle(), VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                      VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);

    check(vkEndCommandBuffer(cmd.handle()));
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

// Mouse button callback - captures mouse button presses and releases
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    // Get current cursor position when button is pressed/released
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);

    if (action == GLFW_PRESS) {
        switch (button) {
        case GLFW_MOUSE_BUTTON_LEFT:
            mouseState.buttons.left = true;
            break;
        case GLFW_MOUSE_BUTTON_RIGHT:
            mouseState.buttons.right = true;
            break;
        case GLFW_MOUSE_BUTTON_MIDDLE:
            mouseState.buttons.middle = true;
            break;
        }
    } else if (action == GLFW_RELEASE) {
        switch (button) {
        case GLFW_MOUSE_BUTTON_LEFT:
            mouseState.buttons.left = false;
            break;
        case GLFW_MOUSE_BUTTON_RIGHT:
            mouseState.buttons.right = false;
            break;
        case GLFW_MOUSE_BUTTON_MIDDLE:
            mouseState.buttons.middle = false;
            break;
        }
    }
}

// Mouse position callback - captures mouse movement
void cursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{
    handleMouseMove(static_cast<int32_t>(xpos), static_cast<int32_t>(ypos));
}

int main()
{
    Window window;

    // Set up callbacks
    window.setKeyCallback(keyCallback);
    window.setMouseButtonCallback(mouseButtonCallback);
    window.setCursorPosCallback(cursorPosCallback);

    VkExtent2D windowSize = window.getFramebufferSize();
    Context ctx(window.getRequiredExtensions(), true);
    Swapchain swapchain(ctx, window.createSurface(ctx.instance()), windowSize, true);

    // Create ShaderManager and GuiRenderer
    ShaderManager shaderManager(ctx, "../../assets/shaders/",
                                {{"gui", {"imgui.vert", "imgui.frag"}}});
    GuiRenderer guiRenderer(ctx, shaderManager, swapchain.colorFormat());

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

    // Initialize ImGui display size
    guiRenderer.resize(windowSize.width, windowSize.height);

    while (!window.isCloseRequested()) {
        window.pollEvents();

        // Update GUI with window size information
        updateGui(windowSize);
        guiRenderer.update();

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

        recordCommandBuffer(commandBuffers_[currentFrame], swapchain, imageIndex, windowSize,
                            guiRenderer);

        // Create semaphore submit infos
        VkSemaphoreSubmitInfo waitSemaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
        waitSemaphoreInfo.semaphore = presentSemaphores_[currentSemaphore];
        waitSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        waitSemaphoreInfo.value = 0;
        waitSemaphoreInfo.deviceIndex = 0;

        VkSemaphoreSubmitInfo signalSemaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
        signalSemaphoreInfo.semaphore = renderSemaphores_[currentSemaphore];
        signalSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        signalSemaphoreInfo.value = 0;
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
