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
      shaderManager_{
          ctx_,
          kShaderPathPrefix,
          {{"gui", {"imgui.vert", "imgui.frag"}}, {"sky", {"skybox.vert.spv", "skybox.frag.spv"}}}},
      guiRenderer_{ctx_, shaderManager_, swapchain_.colorFormat()}, skyTextures_{ctx_}
{
    printLog("Current working directory: {}", std::filesystem::current_path().string());

    // Set up GLFW callbacks
    window_.setKeyCallback(keyCallback);
    window_.setMouseButtonCallback(mouseButtonCallback);
    window_.setCursorPosCallback(cursorPosCallback);
    window_.setScrollCallback(scrollCallback);

    // Store this instance in GLFW user pointer for callback access
    window_.setUserPointer(this);

    // Setup frame resources
    commandBuffers_ = ctx_.createGraphicsCommandBuffers(kMaxFramesInFlight);

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

    // Initialize GUI
    guiRenderer_.resize(windowSize_.width, windowSize_.height);

    // Camera setup
    const float aspectRatio = float(windowSize_.width) / windowSize_.height;
    camera_.type = hlab::Camera::CameraType::firstperson;
    camera_.setPosition(glm::vec3(0.0f, 0.0f, -2.5f));
    camera_.setRotation(glm::vec3(0.0f));
    camera_.updateViewMatrix();
    camera_.setPerspective(75.0f, aspectRatio, 0.1f, 256.0f);

    // Initialize skybox
    initializeSkybox();
}

Ex10_Example::~Ex10_Example()
{
    ctx_.waitIdle();

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

void Ex10_Example::initializeSkybox()
{
    // Create skybox pipeline
    skyPipeline_ = std::make_unique<Pipeline>(ctx_, shaderManager_, "sky", swapchain_.colorFormat(),
                                              ctx_.depthFormat(), VK_SAMPLE_COUNT_1_BIT);

    // Load IBL textures
    string path = kAssetsPathPrefix + "textures/golden_gate_hills_4k/";
    skyTextures_.loadKtxMaps(path + "specularGGX.ktx2", path + "diffuseLambertian.ktx2",
                             path + "outputLUT.png");

    // Create uniform buffers for each frame
    sceneDataUniforms_.clear();
    sceneDataUniforms_.reserve(kMaxFramesInFlight);
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        sceneDataUniforms_.emplace_back(ctx_, sceneDataUBO_);
    }

    optionsUniforms_.clear();
    optionsUniforms_.reserve(kMaxFramesInFlight);
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        optionsUniforms_.emplace_back(ctx_, optionsUBO_);
    }

    // Create descriptor sets for scene data and options (set 0)
    sceneDescriptorSets_.resize(kMaxFramesInFlight);
    for (size_t i = 0; i < kMaxFramesInFlight; i++) {
        sceneDescriptorSets_[i].create(
            ctx_, {sceneDataUniforms_[i].resourceBinding(), optionsUniforms_[i].resourceBinding()});
    }

    // Create descriptor set for skybox textures (set 1)
    skyDescriptorSet_.create(ctx_, {skyTextures_.prefiltered().resourceBinding(),
                                    skyTextures_.irradiance().resourceBinding(),
                                    skyTextures_.brdfLUT().resourceBinding()});
}

void Ex10_Example::mainLoop()
{
    auto lastTime = std::chrono::high_resolution_clock::now();

    while (!window_.isCloseRequested() && !shouldClose_) {
        window_.pollEvents();

        // Calculate delta time for camera updates
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;

        // Clamp delta time to prevent large jumps
        deltaTime = std::min(deltaTime, 0.033f); // Max 33ms (30 FPS minimum)

        // Update camera
        camera_.update(deltaTime);

        // Update scene data UBO
        sceneDataUBO_.projection = camera_.matrices.perspective;
        sceneDataUBO_.view = camera_.matrices.view;
        sceneDataUBO_.cameraPos = camera_.position;
        sceneDataUniforms_[currentFrame_].updateData();
        optionsUniforms_[currentFrame_].updateData();

        updateGui(windowSize_);
        guiRenderer_.update();

        renderFrame();
    }
}

void Ex10_Example::renderFrame()
{
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

    // Add camera info window
    ImGui::SetNextWindowPos(ImVec2(320, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 150), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Camera Control")) {
        ImGui::Text("Camera Position: (%.2f, %.2f, %.2f)", camera_.position.x, camera_.position.y,
                    camera_.position.z);
        ImGui::Text("Camera Rotation: (%.2f, %.2f, %.2f)", camera_.rotation.x, camera_.rotation.y,
                    camera_.rotation.z);

        ImGui::Separator();
        ImGui::Text("Controls:");
        ImGui::Text("Mouse: Look around");
        ImGui::Text("WASD: Move");
        ImGui::Text("QE: Up/Down");
        ImGui::Text("F2: Toggle camera mode");

        bool isFirstPerson = camera_.type == hlab::Camera::CameraType::firstperson;
        if (ImGui::Checkbox("First Person Mode", &isFirstPerson)) {
            camera_.type = isFirstPerson ? hlab::Camera::CameraType::firstperson
                                         : hlab::Camera::CameraType::lookat;
        }
    }
    ImGui::End();

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

    // Set viewport and scissor
    VkViewport viewport{0.0f, 0.0f, (float)windowSize.width, (float)windowSize.height, 0.0f, 1.0f};
    VkRect2D scissor{0, 0, windowSize.width, windowSize.height};
    vkCmdSetViewport(cmd.handle(), 0, 1, &viewport);
    vkCmdSetScissor(cmd.handle(), 0, 1, &scissor);

    // Render skybox
    vkCmdBindPipeline(cmd.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, skyPipeline_->pipeline());

    // Bind descriptor sets: set 0 (scene data), set 1 (skybox textures)
    const auto descriptorSets =
        std::vector{sceneDescriptorSets_[currentFrame_].handle(), skyDescriptorSet_.handle()};

    vkCmdBindDescriptorSets(
        cmd.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, skyPipeline_->pipelineLayout(), 0,
        static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(), 0, nullptr);

    // Draw skybox - 36 vertices from hardcoded data in shader
    vkCmdDraw(cmd.handle(), 36, 1, 0, 0);

    vkCmdEndRendering(cmd.handle());

    // Draw GUI on top of the skybox
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

void Ex10_Example::handleMouseMove(int32_t x, int32_t y)
{
    int32_t dx = (int32_t)mouseState_.position.x - x;
    int32_t dy = (int32_t)mouseState_.position.y - y;

    // Don't handle mouse input if ImGui wants to capture it
    if (ImGui::GetIO().WantCaptureMouse) {
        mouseState_.position = glm::vec2((float)x, (float)y);
        return;
    }

    if (mouseState_.buttons.left) {
        camera_.rotate(glm::vec3(-dy * camera_.rotationSpeed, -dx * camera_.rotationSpeed, 0.0f));
    }

    if (mouseState_.buttons.right) {
        camera_.translate(glm::vec3(-0.0f, 0.0f, dy * .005f));
    }

    if (mouseState_.buttons.middle) {
        camera_.translate(glm::vec3(-dx * 0.005f, dy * 0.005f, 0.0f));
    }

    mouseState_.position = glm::vec2((float)x, (float)y);
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

void Ex10_Example::scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    Ex10_Example* example = static_cast<Ex10_Example*>(glfwGetWindowUserPointer(window));
    if (example) {
        example->handleScroll(xoffset, yoffset);
    }
}

// Instance callback handlers
void Ex10_Example::handleKeyInput(int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS) {
        switch (key) {
        case GLFW_KEY_ESCAPE:
            shouldClose_ = true;
            break;
        case GLFW_KEY_F2:
            if (camera_.type == hlab::Camera::CameraType::lookat) {
                camera_.type = hlab::Camera::CameraType::firstperson;
            } else {
                camera_.type = hlab::Camera::CameraType::lookat;
            }
            break;
        }

        // First person camera controls
        if (camera_.type == hlab::Camera::firstperson) {
            switch (key) {
            case GLFW_KEY_W:
                camera_.keys.forward = true;
                break;
            case GLFW_KEY_S:
                camera_.keys.backward = true;
                break;
            case GLFW_KEY_A:
                camera_.keys.left = true;
                break;
            case GLFW_KEY_D:
                camera_.keys.right = true;
                break;
            case GLFW_KEY_E:
                camera_.keys.down = true;
                break;
            case GLFW_KEY_Q:
                camera_.keys.up = true;
                break;
            }
        }
    } else if (action == GLFW_RELEASE) {
        // First person camera controls
        if (camera_.type == hlab::Camera::firstperson) {
            switch (key) {
            case GLFW_KEY_W:
                camera_.keys.forward = false;
                break;
            case GLFW_KEY_S:
                camera_.keys.backward = false;
                break;
            case GLFW_KEY_A:
                camera_.keys.left = false;
                break;
            case GLFW_KEY_D:
                camera_.keys.right = false;
                break;
            case GLFW_KEY_E:
                camera_.keys.down = false;
                break;
            case GLFW_KEY_Q:
                camera_.keys.up = false;
                break;
            }
        }
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
    handleMouseMove(static_cast<int32_t>(xpos), static_cast<int32_t>(ypos));
}

void Ex10_Example::handleScroll(double xoffset, double yoffset)
{
    camera_.translate(glm::vec3(0.0f, 0.0f, (float)yoffset * 0.05f));
}