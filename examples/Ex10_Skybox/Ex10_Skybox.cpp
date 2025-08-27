#include "engine/Context.h"
#include "engine/Window.h"
#include "engine/Swapchain.h"
#include "engine/CommandBuffer.h"
#include "engine/GuiRenderer.h"
#include "engine/ShaderManager.h"
#include "engine/Logger.h"

#include <chrono>
#include <thread>
#include <filesystem>
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
glm::vec4 clearColor = glm::vec4(0.2f, 0.3f, 0.5f, 1.0f); // Default blue-ish color with alpha
MouseState mouseState;

// Forward declarations
void renderColorControlWindow();
void renderColorPresets();

// Separate preset rendering for clarity
void renderColorPresets()
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
            clearColor = presets[i].color;
        }

        if ((i + 1) % buttonsPerRow != 0 && i < std::size(presets) - 1) {
            ImGui::SameLine();
        }
    }
}

// Separate the GUI window rendering for better organization
void renderColorControlWindow()
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
    ImGui::ColorEdit4("Clear Color", &clearColor.r);

    ImGui::Separator();
    ImGui::Text("Individual Controls:");

    // RGBA sliders
    ImGui::SliderFloat("Red", &clearColor.r, 0.0f, 1.0f, "%.3f");
    ImGui::SliderFloat("Green", &clearColor.g, 0.0f, 1.0f, "%.3f");
    ImGui::SliderFloat("Blue", &clearColor.b, 0.0f, 1.0f, "%.3f");
    ImGui::SliderFloat("Alpha", &clearColor.a, 0.0f, 1.0f, "%.3f");

    ImGui::Separator();
    ImGui::Text("Color Preview:");

    // Preview button
    ImGui::ColorButton("Preview", ImVec4(clearColor.r, clearColor.g, clearColor.b, clearColor.a), 0,
                       ImVec2(50, 50));

    ImGui::Separator();
    ImGui::Text("Presets:");

    // Preset buttons with better layout
    renderColorPresets();

    ImGui::End();
}

// Enhanced GUI function with better structure
void updateGui(VkExtent2D windowSize)
{
    ImGuiIO& io = ImGui::GetIO();

    // Update ImGui IO state
    io.DisplaySize = ImVec2(float(windowSize.width), float(windowSize.height));
    io.MousePos = ImVec2(mouseState.position.x, mouseState.position.y);
    io.MouseDown[0] = mouseState.buttons.left;
    io.MouseDown[1] = mouseState.buttons.right;
    io.MouseDown[2] = mouseState.buttons.middle;

    // Begin GUI frame
    ImGui::NewFrame();

    // Render color control window
    renderColorControlWindow();

    ImGui::Render();
}

// Helper functions for better organization
void initializeSynchronization(Context& ctx, uint32_t maxFramesInFlight, uint32_t imageCount,
                               vector<VkSemaphore>& presentSemaphores,
                               vector<VkSemaphore>& renderSemaphores,
                               vector<VkFence>& inFlightFences)
{
    // Create semaphores
    presentSemaphores.resize(imageCount);
    renderSemaphores.resize(imageCount);
    for (size_t i = 0; i < imageCount; i++) {
        VkSemaphoreCreateInfo semaphoreCI{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        check(vkCreateSemaphore(ctx.device(), &semaphoreCI, nullptr, &presentSemaphores[i]));
        check(vkCreateSemaphore(ctx.device(), &semaphoreCI, nullptr, &renderSemaphores[i]));
    }

    // Create fences
    inFlightFences.resize(maxFramesInFlight);
    for (size_t i = 0; i < maxFramesInFlight; i++) {
        VkFenceCreateInfo fenceCreateInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        check(vkCreateFence(ctx.device(), &fenceCreateInfo, nullptr, &inFlightFences[i]));
    }
}

void cleanupSynchronization(Context& ctx, vector<VkSemaphore>& presentSemaphores,
                            vector<VkSemaphore>& renderSemaphores, vector<VkFence>& inFlightFences)
{
    for (auto& semaphore : presentSemaphores) {
        vkDestroySemaphore(ctx.device(), semaphore, nullptr);
    }
    for (auto& semaphore : renderSemaphores) {
        vkDestroySemaphore(ctx.device(), semaphore, nullptr);
    }
    for (auto& fence : inFlightFences) {
        vkDestroyFence(ctx.device(), fence, nullptr);
    }
}

void submitFrame(CommandBuffer& commandBuffer, VkSemaphore waitSemaphore,
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

    // Use clearColor directly - no need for conversion function
    VkClearColorValue clearColorValue = {{clearColor.r, clearColor.g, clearColor.b, clearColor.a}};

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

// Mouse position callback - captures mouse movement and updates position directly
void cursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{
    // Update mouse position directly - no need for separate handleMouseMove function
    mouseState.position = glm::vec2(static_cast<float>(xpos), static_cast<float>(ypos));

    // Note: In a full application, you might check ImGui::GetIO().WantCaptureMouse here
    // to determine if mouse events should be handled by ImGui or the application
}

int main()
{
    Window window;
    window.setKeyCallback(keyCallback);
    window.setMouseButtonCallback(mouseButtonCallback);
    window.setCursorPosCallback(cursorPosCallback);

    VkExtent2D windowSize = window.getFramebufferSize();
    Context ctx(window.getRequiredExtensions(), true);
    Swapchain swapchain(ctx, window.createSurface(ctx.instance()), windowSize, true);

    printLog("Current working directory: {}", std::filesystem::current_path().string());

    const string kAssetsPathPrefix = "../../assets/";
    const string kShaderPathPrefix = kAssetsPathPrefix + "shaders/";

    ShaderManager shaderManager(ctx, kShaderPathPrefix, {{"gui", {"imgui.vert", "imgui.frag"}}});
    GuiRenderer guiRenderer(ctx, shaderManager, swapchain.colorFormat());

    // Setup frame resources
    const uint32_t MAX_FRAMES_IN_FLIGHT = 2;
    vector<CommandBuffer> commandBuffers = ctx.createGraphicsCommandBuffers(MAX_FRAMES_IN_FLIGHT);

    vector<VkSemaphore> presentSemaphores;
    vector<VkSemaphore> renderSemaphores;
    vector<VkFence> inFlightFences;

    initializeSynchronization(ctx, MAX_FRAMES_IN_FLIGHT, swapchain.imageCount(), presentSemaphores,
                              renderSemaphores, inFlightFences);

    uint32_t currentFrame = 0;
    uint32_t currentSemaphore = 0;

    // Initialize GUI
    guiRenderer.resize(windowSize.width, windowSize.height);

    while (!window.isCloseRequested()) {
        window.pollEvents();

        updateGui(windowSize);

        guiRenderer.update();

        check(vkWaitForFences(ctx.device(), 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX));
        check(vkResetFences(ctx.device(), 1, &inFlightFences[currentFrame]));

        uint32_t imageIndex = 0;
        VkResult acquireResult =
            swapchain.acquireNextImage(presentSemaphores[currentSemaphore], imageIndex);

        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
            exitWithMessage("Window resize not implemented");
        } else if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
            exitWithMessage("Failed to acquire swapchain image!");
        }

        recordCommandBuffer(commandBuffers[currentFrame], swapchain, imageIndex, windowSize,
                            guiRenderer);

        submitFrame(commandBuffers[currentFrame], presentSemaphores[currentSemaphore],
                    renderSemaphores[currentSemaphore], inFlightFences[currentFrame]);

        // Present frame
        VkResult presentResult = swapchain.queuePresent(ctx.graphicsQueue(), imageIndex,
                                                        renderSemaphores[currentSemaphore]);

        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
            exitWithMessage("Window resize not implemented");
        } else if (presentResult != VK_SUCCESS) {
            exitWithMessage("Failed to present swapchain image!");
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        currentSemaphore = (currentSemaphore + 1) % swapchain.imageCount();
    }

    ctx.waitIdle();

    cleanupSynchronization(ctx, presentSemaphores, renderSemaphores, inFlightFences);

    return 0;
}
