#include "Application.h"
#include "Logger.h"

#include <format>
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>

namespace hlab {

// Default constructor - uses hardcoded configuration
Application::Application() : Application(ApplicationConfig::createDefault())
{
}

// Configuration-based constructor
Application::Application(const ApplicationConfig& config)
    : window_(), windowSize_(window_.getFramebufferSize()),
      ctx_(window_.getRequiredExtensions(), true),
      swapchain_(ctx_, window_.createSurface(ctx_.instance()), windowSize_),
      shaderManager_(ctx_, kShaderPathPrefix,
                     {{"shadowMap", {"shadowMap.vert.spv", "shadowMap.frag.spv"}},
                      {"pbrForward", {"pbrForward.vert.spv", "pbrForward.frag.spv"}},
                      {"sky", {"skybox.vert.spv", "skybox.frag.spv"}},
                      {"ssao", {"ssao.comp.spv"}},
                      {"post", {"post.vert.spv", "post.frag.spv"}},
                      {"gui", {"imgui.vert", "imgui.frag"}}}),
      guiRenderer_(ctx_, shaderManager_, swapchain_.colorFormat()),
      renderer_(ctx_, shaderManager_, kMaxFramesInFlight, kAssetsPathPrefix, kShaderPathPrefix)
{
    initializeVulkanResources();
    setupCallbacks();
    initializeWithConfig(config);
}

// Future: Load from file constructor
Application::Application(const string& configFile)
    : Application(ApplicationConfig::createDefault()) // Fallback to default
{
    printLog("Config file loading not implemented yet, using default configuration");
}

void Application::initializeVulkanResources()
{
    msaaSamples_ = ctx_.getMaxUsableSampleCount();
    commandBuffers_ = ctx_.createGraphicsCommandBuffers(kMaxFramesInFlight);

    // Initialize fences
    waitFences_.resize(kMaxFramesInFlight);
    for (auto& fence : waitFences_) {
        VkFenceCreateInfo fenceCreateInfo{};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        check(vkCreateFence(ctx_.device(), &fenceCreateInfo, nullptr, &fence));
    }

    // Initialize semaphores
    presentCompleteSemaphores_.resize(swapchain_.images().size());
    renderCompleteSemaphores_.resize(swapchain_.images().size());
    for (size_t i = 0; i < swapchain_.images().size(); i++) {
        VkSemaphoreCreateInfo semaphoreCI{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        check(vkCreateSemaphore(ctx_.device(), &semaphoreCI, nullptr,
                                &presentCompleteSemaphores_[i]));
        check(
            vkCreateSemaphore(ctx_.device(), &semaphoreCI, nullptr, &renderCompleteSemaphores_[i]));
    }
}

void Application::initializeWithConfig(const ApplicationConfig& config)
{
    setupCamera(config.camera);
    loadModels(config.models);

    renderer_.prepareForModels(models_, swapchain_.colorFormat(), ctx_.depthFormat(), msaaSamples_,
                               windowSize_.width, windowSize_.height);
}

void Application::setupCamera(const CameraConfig& cameraConfig)
{
    const float aspectRatio = float(windowSize_.width) / windowSize_.height;

    camera_.type = cameraConfig.type;
    camera_.position = cameraConfig.position;
    camera_.rotation = cameraConfig.rotation;
    camera_.viewPos = cameraConfig.viewPos;
    camera_.setMovementSpeed(cameraConfig.movementSpeed);
    camera_.setRotationSpeed(cameraConfig.rotationSpeed);

    camera_.updateViewMatrix();
    camera_.setPerspective(cameraConfig.fov, aspectRatio, cameraConfig.nearPlane,
                           cameraConfig.farPlane);
}

void Application::loadModels(const vector<ModelConfig>& modelConfigs)
{
    for (const auto& modelConfig : modelConfigs) {
        models_.emplace_back(ctx_);
        auto& model = models_.back();

        string fullPath = kAssetsPathPrefix + modelConfig.filePath;
        model.loadFromModelFile(fullPath, modelConfig.isBistroObj);
        model.name() = modelConfig.displayName;
        model.modelMatrix() = modelConfig.transform;

        // Setup animation if model supports it
        if (model.hasAnimations() && modelConfig.autoPlayAnimation) {
            printLog("Found {} animations in model '{}'", model.getAnimationCount(),
                     modelConfig.displayName);

            if (model.getAnimationCount() > 0) {
                uint32_t animIndex =
                    std::min(modelConfig.initialAnimationIndex, model.getAnimationCount() - 1);
                model.setAnimationIndex(animIndex);
                model.setAnimationLooping(modelConfig.loopAnimation);
                model.setAnimationSpeed(modelConfig.animationSpeed);
                model.playAnimation();

                printLog("Started animation: '{}'",
                         model.getAnimation()->getCurrentAnimationName());
                printLog("Animation duration: {:.2f} seconds", model.getAnimation()->getDuration());
            }
        } else if (!model.hasAnimations()) {
            printLog("No animations found in model '{}'", modelConfig.displayName);
        }
    }
}

void Application::setupCallbacks()
{
    window_.setUserPointer(this);

    // Keyboard/Mouse callbacks

    window_.setKeyCallback([](GLFWwindow* window, int key, int scancode, int action, int mods) {
        auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));

        if (action == GLFW_PRESS) {

            // General controls
            switch (key) {
            case GLFW_KEY_P:
                break;
            case GLFW_KEY_F1:
                break;
            case GLFW_KEY_F2:
                if (app->camera_.type == hlab::Camera::CameraType::lookat) {
                    app->camera_.type = hlab::Camera::CameraType::firstperson;
                } else {
                    app->camera_.type = hlab::Camera::CameraType::lookat;
                }
                break;
            case GLFW_KEY_F3:
                printLog("{} {} {}", glm::to_string(app->camera_.position),
                         glm::to_string(app->camera_.rotation),
                         glm::to_string(app->camera_.viewPos));
                break;
            case GLFW_KEY_ESCAPE:
                glfwSetWindowShouldClose(window, GLFW_TRUE);
                break;
            }

            // First person camera controls
            if (app->camera_.type == hlab::Camera::firstperson) {
                switch (key) {
                case GLFW_KEY_W:
                    app->camera_.keys.forward = true;
                    break;
                case GLFW_KEY_S:
                    app->camera_.keys.backward = true;
                    break;
                case GLFW_KEY_A:
                    app->camera_.keys.left = true;
                    break;
                case GLFW_KEY_D:
                    app->camera_.keys.right = true;
                    break;
                case GLFW_KEY_E:
                    app->camera_.keys.down = true;
                    break;
                case GLFW_KEY_Q:
                    app->camera_.keys.up = true;
                    break;
                }
            }

            // NEW: Handle animation control keys
            switch (key) {
            case GLFW_KEY_SPACE:
                // Toggle animation play/pause
                for (auto& model : app->models_) {
                    if (model.hasAnimations()) {
                        if (model.isAnimationPlaying()) {
                            model.pauseAnimation();
                            printLog("Animation paused");
                        } else {
                            model.playAnimation();
                            printLog("Animation resumed");
                        }
                    }
                }
                break;

            case GLFW_KEY_R:
                // Restart animation
                for (auto& model : app->models_) {
                    if (model.hasAnimations()) {
                        model.stopAnimation();
                        model.playAnimation();
                        printLog("Animation restarted");
                    }
                }
                break;

            case GLFW_KEY_1:
            case GLFW_KEY_2:
            case GLFW_KEY_3:
            case GLFW_KEY_4:
            case GLFW_KEY_5:
                // Switch between animations (1-5)
                {
                    uint32_t animIndex = key - GLFW_KEY_1;
                    for (auto& model : app->models_) {
                        if (model.hasAnimations() && animIndex < model.getAnimationCount()) {
                            model.setAnimationIndex(animIndex);
                            model.playAnimation();
                            printLog("Switched to animation {}: '{}'", animIndex,
                                     model.getAnimation()->getCurrentAnimationName());
                        }
                    }
                }
                break;
            }
        } else if (action == GLFW_RELEASE) {
            // First person camera controls
            if (app->camera_.type == hlab::Camera::firstperson) {
                switch (key) {
                case GLFW_KEY_W:
                    app->camera_.keys.forward = false;
                    break;
                case GLFW_KEY_S:
                    app->camera_.keys.backward = false;
                    break;
                case GLFW_KEY_A:
                    app->camera_.keys.left = false;
                    break;
                case GLFW_KEY_D:
                    app->camera_.keys.right = false;
                    break;
                case GLFW_KEY_E:
                    app->camera_.keys.down = false;
                    break;
                case GLFW_KEY_Q:
                    app->camera_.keys.up = false;
                    break;
                }
            }
        }
    });

    window_.setMouseButtonCallback([](GLFWwindow* window, int button, int action, int mods) {
        auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));

        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        if (action == GLFW_PRESS) {
            switch (button) {
            case GLFW_MOUSE_BUTTON_LEFT:
                app->mouseState_.buttons.left = true;
                break;
            case GLFW_MOUSE_BUTTON_RIGHT:
                app->mouseState_.buttons.right = true;
                break;
            case GLFW_MOUSE_BUTTON_MIDDLE:
                app->mouseState_.buttons.middle = true;
                break;
            }
        } else if (action == GLFW_RELEASE) {
            switch (button) {
            case GLFW_MOUSE_BUTTON_LEFT:
                app->mouseState_.buttons.left = false;
                break;
            case GLFW_MOUSE_BUTTON_RIGHT:
                app->mouseState_.buttons.right = false;
                break;
            case GLFW_MOUSE_BUTTON_MIDDLE:
                app->mouseState_.buttons.middle = false;
                break;
            }
        }
    });

    window_.setCursorPosCallback([](GLFWwindow* window, double xpos, double ypos) {
        auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
        app->handleMouseMove(static_cast<int32_t>(xpos), static_cast<int32_t>(ypos));
    });

    window_.setScrollCallback([](GLFWwindow* window, double xoffset, double yoffset) {
        auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
        app->camera_.translate(glm::vec3(0.0f, 0.0f, (float)yoffset * 0.05f));
    });

    // Add framebuffer size callback
    window_.setFramebufferSizeCallback([](GLFWwindow* window, int width, int height) {
        exitWithMessage("Window resize not implemented");
    });
}

Application::~Application()
{
    for (auto& cmd : commandBuffers_) {
        cmd.cleanup();
    }

    for (size_t i = 0; i < swapchain_.images().size(); i++) {
        vkDestroySemaphore(ctx_.device(), presentCompleteSemaphores_[i], nullptr);
        vkDestroySemaphore(ctx_.device(), renderCompleteSemaphores_[i], nullptr);
    }

    for (auto& fence : waitFences_) {
        vkDestroyFence(ctx_.device(), fence, nullptr);
    }

    // Destructors of members automatically cleanup everything.
}

void Application::run()
{
    // 파이프라인은 어떤 레이아웃으로 리소스가 들어와야 하는지는 알고 있지만
    // 구체적으로 어떤 리소스가 들어올지를 직접 결정하지는 않는다.
    // 렌더러가 파이프라인을 사용할 때 어떤 리소스를 넣을지 결정한다.

    uint32_t frameCounter = 0;
    uint32_t currentFrame = 0;     // For CPU resources (command buffers, fences)
    uint32_t currentSemaphore = 0; // For GPU semaphores (swapchain sync)

    // NEW: Animation timing variables
    auto lastTime = std::chrono::high_resolution_clock::now();
    float deltaTime = 0.016f; // Default to ~60 FPS

    while (!window_.isCloseRequested()) {
        window_.pollEvents();

        // NEW: Calculate delta time for smooth animation
        auto currentTime = std::chrono::high_resolution_clock::now();
        deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;

        // Clamp delta time to prevent large jumps (e.g., when debugging)
        deltaTime = std::min(deltaTime, 0.033f); // Max 33ms (30 FPS minimum)

        updateGui();

        camera_.update(deltaTime);
        renderer_.sceneUBO().projection = camera_.matrices.perspective;
        renderer_.sceneUBO().view = camera_.matrices.view;
        renderer_.sceneUBO().cameraPos = camera_.position;

        for (auto& model : models_) {
            if (model.hasAnimations()) {
                model.updateAnimation(deltaTime);
            }
        }

        // Update for shadow mapping
        {
            if (models_.size() > 0) {

                glm::mat4 lightView =
                    glm::lookAt(vec3(0.0f), -renderer_.sceneUBO().directionalLightDir,
                                glm::vec3(0.0f, 0.0f, 1.0f));

                // Transform the first model's bounding box by its model matrix
                vec3 firstMin =
                    vec3(models_[0].modelMatrix() * vec4(models_[0].boundingBoxMin(), 1.0f));
                vec3 firstMax =
                    vec3(models_[0].modelMatrix() * vec4(models_[0].boundingBoxMax(), 1.0f));

                // Ensure min is actually smaller than max for each component
                vec3 min_ = glm::min(firstMin, firstMax);
                vec3 max_ = glm::max(firstMin, firstMax);

                // Iterate through all remaining models to determine the combined bounding box
                for (uint32_t i = 1; i < models_.size(); i++) {
                    // Transform this model's bounding box by its model matrix
                    vec3 modelMin =
                        vec3(models_[i].modelMatrix() * vec4(models_[i].boundingBoxMin(), 1.0f));
                    vec3 modelMax =
                        vec3(models_[i].modelMatrix() * vec4(models_[i].boundingBoxMax(), 1.0f));

                    // Ensure proper min/max ordering
                    vec3 transformedMin = glm::min(modelMin, modelMax);
                    vec3 transformedMax = glm::max(modelMin, modelMax);

                    // Expand the overall bounding box
                    min_ = glm::min(min_, transformedMin);
                    max_ = glm::max(max_, transformedMax);
                }

                vec3 corners[] = {
                    vec3(min_.x, min_.y, min_.z), vec3(min_.x, max_.y, min_.z),
                    vec3(min_.x, min_.y, max_.z), vec3(min_.x, max_.y, max_.z),
                    vec3(max_.x, min_.y, min_.z), vec3(max_.x, max_.y, min_.z),
                    vec3(max_.x, min_.y, max_.z), vec3(max_.x, max_.y, max_.z),
                };
                vec3 vmin(std::numeric_limits<float>::max());
                vec3 vmax(std::numeric_limits<float>::lowest());
                for (size_t i = 0; i != 8; i++) {
                    auto temp = vec3(lightView * vec4(corners[i], 1.0f));
                    vmin = glm::min(vmin, temp);
                    vmax = glm::max(vmax, temp);
                }
                min_ = vmin;
                max_ = vmax;
                glm::mat4 lightProjection = glm::orthoLH_ZO(min_.x, max_.x, min_.y, max_.y, max_.z,
                                                            min_.z); // 마지막 Max, Min 순서 주의
                renderer_.sceneUBO().lightSpaceMatrix = lightProjection * lightView;

                // Modifed "Vulkan 3D Graphics Rendering Cookbook - 2nd Edition Build Status"
                // https://github.com/PacktPublishing/3D-Graphics-Rendering-Cookbook-Second-Edition
            }
        }

        // Wait using currentFrame index (CPU-side fence)
        check(vkWaitForFences(ctx_.device(), 1, &waitFences_[currentFrame], VK_TRUE, UINT64_MAX));
        check(vkResetFences(ctx_.device(), 1, &waitFences_[currentFrame]));

        renderer_.update(camera_, currentFrame, (float)glfwGetTime() * 0.5f);
        renderer_.updateBoneData(models_, currentFrame);
        guiRenderer_.update();

        // Acquire using currentSemaphore index (GPU-side semaphore)
        uint32_t imageIndex{0};
        VkResult result = vkAcquireNextImageKHR(ctx_.device(), swapchain_.handle(), UINT64_MAX,
                                                presentCompleteSemaphores_[currentSemaphore],
                                                VK_NULL_HANDLE, &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            continue; // Ignore resize in this example
        } else if ((result != VK_SUCCESS) && (result != VK_SUBOPTIMAL_KHR)) {
            exitWithMessage("Could not acquire the next swap chain image!");
        }

        // Use currentFrame index (CPU-side command buffer)
        CommandBuffer& cmd = commandBuffers_[currentFrame];

        // Begin command buffer
        vkResetCommandBuffer(cmd.handle(), 0);
        VkCommandBufferBeginInfo cmdBufferBeginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        check(vkBeginCommandBuffer(cmd.handle(), &cmdBufferBeginInfo));

        // Make Shadow map
        {
            renderer_.makeShadowMap(cmd.handle(), currentFrame, models_);
        }

        {
            // Transition swapchain image from undefined to color attachment layout
            swapchain_.barrierHelper(imageIndex)
                .transitionTo(cmd.handle(), VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                              VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

            VkViewport viewport{0.0f, 0.0f, (float)windowSize_.width, (float)windowSize_.height,
                                0.0f, 1.0f};
            VkRect2D scissor{0, 0, windowSize_.width, windowSize_.height};

            // Draw models
            {
                renderer_.draw(cmd.handle(), currentFrame, swapchain_.imageView(imageIndex),
                               models_, viewport, scissor);
            }

            // Draw GUI (overwrite to swapchain image)
            {
                guiRenderer_.draw(cmd.handle(), swapchain_.imageView(imageIndex), viewport);
            }

            swapchain_.barrierHelper(imageIndex)
                .transitionTo(cmd.handle(), VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                              VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);
        }
        check(vkEndCommandBuffer(cmd.handle())); // End command buffer

        VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        // 비교: 마지막으로 실행되는 셰이더가 Compute라면 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT

        VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submitInfo.pCommandBuffers = &cmd.handle();
        submitInfo.commandBufferCount = 1;
        submitInfo.pWaitDstStageMask = &waitStageMask;
        submitInfo.pWaitSemaphores = &presentCompleteSemaphores_[currentSemaphore];
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &renderCompleteSemaphores_[currentSemaphore];
        submitInfo.signalSemaphoreCount = 1;
        check(vkQueueSubmit(cmd.queue(), 1, &submitInfo, waitFences_[currentFrame]));

        VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &renderCompleteSemaphores_[currentSemaphore];
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchain_.handle();
        presentInfo.pImageIndices = &imageIndex;
        check(vkQueuePresentKHR(ctx_.graphicsQueue(), &presentInfo));

        currentFrame = (currentFrame + 1) % kMaxFramesInFlight;
        currentSemaphore = (currentSemaphore + 1) % swapchain_.imageCount();

        frameCounter++;
    }

    ctx_.waitIdle(); // 종료하기 전 GPU 사용이 모두 끝날때까지 대기
}

void Application::updateGui()
{
    static float scale = 1.4f;

    ImGuiIO& io = ImGui::GetIO();

    io.DisplaySize = ImVec2(float(windowSize_.width), float(windowSize_.height));
    // io.DeltaTime = frameTimer;

    // Always pass mouse input to ImGui - let ImGui decide if it wants to capture it
    io.MousePos = ImVec2(mouseState_.position.x, mouseState_.position.y);
    io.MouseDown[0] = mouseState_.buttons.left;
    io.MouseDown[1] = mouseState_.buttons.right;
    io.MouseDown[2] = mouseState_.buttons.middle;

    ImGui::NewFrame();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::SetNextWindowPos(ImVec2(10 * scale, 10 * scale), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiCond_FirstUseEver);
    ImGui::Begin("벌컨 실시간 렌더링 예제", nullptr, ImGuiWindowFlags_None);

    static vec3 lightColor = vec3(1.0f);
    static float lightIntensity = 28.454f;
    ImGui::SliderFloat("Light Intensity", &lightIntensity, 0.0f, 100.0f);
    renderer_.sceneUBO().directionalLightColor = lightIntensity * lightColor;

    // TODO: IS there a way to determine directionalLightColor from time of day? bright morning sun
    // to noon white light to golden sunset color.

    static float elevation = 65.2f; // Elevation angle (up/down) in degrees
    static float azimuth = -143.8f; // Azimuth angle (left/right) in degrees

    ImGui::SliderFloat("Light Elevation", &elevation, -90.0f, 90.0f, "%.1f°");
    ImGui::SliderFloat("Light Azimuth", &azimuth, -180.0f, 180.0f, "%.1f°");

    // Convert to radians
    float elev_rad = glm::radians(elevation);
    float azim_rad = glm::radians(azimuth);

    // Calculate direction using standard spherical coordinates
    glm::vec3 lightDir;
    lightDir.x = cos(elev_rad) * sin(azim_rad);
    lightDir.y = sin(elev_rad);
    lightDir.z = cos(elev_rad) * cos(azim_rad);

    // Set the light direction (already normalized from spherical coordinates)
    renderer_.sceneUBO().directionalLightDir = lightDir;

    // Display current light direction for debugging
    ImGui::Text("Light Dir: (%.2f, %.2f, %.2f)", renderer_.sceneUBO().directionalLightDir.x,
                renderer_.sceneUBO().directionalLightDir.y,
                renderer_.sceneUBO().directionalLightDir.z);

    // Rendering Options Controls
    ImGui::Separator();
    ImGui::Text("Rendering Options");

    bool textureOn = renderer_.optionsUBO().textureOn != 0;
    bool shadowOn = renderer_.optionsUBO().shadowOn != 0;
    bool discardOn = renderer_.optionsUBO().discardOn != 0;
    // bool animationOn = renderer_.optionsUBO().animationOn != 0;

    if (ImGui::Checkbox("Textures", &textureOn)) {
        renderer_.optionsUBO().textureOn = textureOn ? 1 : 0;
    }
    if (ImGui::Checkbox("Shadows", &shadowOn)) {
        renderer_.optionsUBO().shadowOn = shadowOn ? 1 : 0;
    }
    if (ImGui::Checkbox("Alpha Discard", &discardOn)) {
        renderer_.optionsUBO().discardOn = discardOn ? 1 : 0;
    }
    // if (ImGui::Checkbox("Animation", &animationOn)) {
    //     renderer_.optionsUBO().animationOn = animationOn ? 1 : 0;
    // }

    ImGui::Separator();

    for (uint32_t i = 0; i < models_.size(); i++) {
        auto& m = models_[i];
        ImGui::Checkbox(std::format("{}##{}", m.name(), i).c_str(), &m.visible());

        // clean
        ImGui::SliderFloat(format("SpecularWeight##{}", i).c_str(), &(m.coeffs()[0]), 0.0f, 1.0f);
        ImGui::SliderFloat(format("DiffuseWeight##{}", i).c_str(), &(m.coeffs()[1]), 0.0f, 10.0f);
        ImGui::SliderFloat(format("EmissiveWeight##{}", i).c_str(), &(m.coeffs()[2]), 0.0f, 10.0f);
        ImGui::SliderFloat(format("ShadowOffset##{}", i).c_str(), &(m.coeffs()[3]), 0.0f, 1.0f);
        ImGui::SliderFloat(format("RoughnessWeight##{}", i).c_str(), &(m.coeffs()[4]), 0.0f, 1.0f);
        ImGui::SliderFloat(format("MetallicWeight##{}", i).c_str(), &(m.coeffs()[5]), 0.0f, 1.0f);

        // Extract and edit position
        glm::vec3 position = glm::vec3(m.modelMatrix()[3]);
        if (ImGui::SliderFloat3(std::format("Position##{}", i).c_str(), &position.x, -10.0f,
                                10.0f)) {
            m.modelMatrix()[3] = glm::vec4(position, 1.0f);
        }

        // Decompose matrix into components
        glm::vec3 scale, translation, skew;
        glm::vec4 perspective;
        glm::quat rotation;

        if (glm::decompose(m.modelMatrix(), scale, rotation, translation, skew, perspective)) {
            // Convert quaternion to euler angles for easier editing
            glm::vec3 eulerAngles = glm::eulerAngles(rotation);
            float yRotationDegrees = glm::degrees(eulerAngles.y);

            if (ImGui::SliderFloat(std::format("Y Rotation##{}", i).c_str(), &yRotationDegrees,
                                   -90.0f, 90.0f, "%.1f°")) {
                // Reconstruct matrix from components
                eulerAngles.y = glm::radians(yRotationDegrees);
                rotation = glm::quat(eulerAngles);

                glm::mat4 T = glm::translate(glm::mat4(1.0f), translation);
                glm::mat4 R = glm::mat4_cast(rotation);
                glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);

                m.modelMatrix() = T * R * S;
            }
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();

    renderHDRControlWindow();

    renderPostProcessingControlWindow();

    ImGui::Render();
}

// ADD: HDR Control window method (based on Ex10_Example)
void Application::renderHDRControlWindow()
{
    ImGui::SetNextWindowPos(ImVec2(320, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 350), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("HDR Skybox Controls")) {
        ImGui::End();
        return;
    }

    // HDR Environment Controls
    if (ImGui::CollapsingHeader("HDR Environment", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Environment Intensity", &renderer_.skyOptionsUBO().environmentIntensity,
                           0.0f, 10.0f, "%.2f");
    }

    // Environment Map Controls
    if (ImGui::CollapsingHeader("Environment Map", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Roughness Level", &renderer_.skyOptionsUBO().roughnessLevel, 0.0f, 8.0f,
                           "%.1f");

        bool useIrradiance = renderer_.skyOptionsUBO().useIrradianceMap != 0;
        if (ImGui::Checkbox("Use Irradiance Map", &useIrradiance)) {
            renderer_.skyOptionsUBO().useIrradianceMap = useIrradiance ? 1 : 0;
        }
        ImGui::SameLine();
        if (ImGui::Button("?")) {
            // Optional: Add click action here if needed
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Toggle between prefiltered environment map (sharp reflections) and "
                              "irradiance map (diffuse lighting)");
        }
    }

    // Debug Visualization
    if (ImGui::CollapsingHeader("Debug Visualization")) {
        bool showMipLevels = renderer_.skyOptionsUBO().showMipLevels != 0;
        if (ImGui::Checkbox("Show Mip Levels", &showMipLevels)) {
            renderer_.skyOptionsUBO().showMipLevels = showMipLevels ? 1 : 0;
        }

        bool showCubeFaces = renderer_.skyOptionsUBO().showCubeFaces != 0;
        if (ImGui::Checkbox("Show Cube Faces", &showCubeFaces)) {
            renderer_.skyOptionsUBO().showCubeFaces = showCubeFaces ? 1 : 0;
        }
    }

    // Simplified Presets
    if (ImGui::CollapsingHeader("Presets")) {
        if (ImGui::Button("Default")) {
            renderer_.skyOptionsUBO().environmentIntensity = 1.0f;
            renderer_.skyOptionsUBO().roughnessLevel = 0.5f;
            renderer_.skyOptionsUBO().useIrradianceMap = 0;
            renderer_.skyOptionsUBO().showMipLevels = 0;
            renderer_.skyOptionsUBO().showCubeFaces = 0;
        }
        ImGui::SameLine();
        if (ImGui::Button("High Exposure")) {
            renderer_.skyOptionsUBO().environmentIntensity = 1.5f;
        }
        ImGui::SameLine();
        if (ImGui::Button("Low Exposure")) {
            renderer_.skyOptionsUBO().environmentIntensity = 0.8f;
        }

        if (ImGui::Button("Sharp Reflections")) {
            renderer_.skyOptionsUBO().roughnessLevel = 0.0f;
            renderer_.skyOptionsUBO().useIrradianceMap = 0;
        }
        ImGui::SameLine();
        if (ImGui::Button("Diffuse Lighting")) {
            renderer_.skyOptionsUBO().useIrradianceMap = 1;
        }
    }

    ImGui::End();
}

// ADD: Post-Processing Control window method (based on Ex11_PostProcessingExample)
void Application::renderPostProcessingControlWindow()
{
    ImGui::SetNextWindowPos(ImVec2(680, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 600), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Post-Processing Controls")) {
        ImGui::End();
        return;
    }

    // Tone Mapping Controls
    if (ImGui::CollapsingHeader("Tone Mapping", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* toneMappingNames[] = {"None",        "Reinhard",          "ACES",
                                          "Uncharted 2", "GT (Gran Turismo)", "Lottes",
                                          "Exponential", "Reinhard Extended", "Luminance",
                                          "Hable"};
        ImGui::Combo("Tone Mapping Type", &renderer_.postOptionsUBO().toneMappingType,
                     toneMappingNames, IM_ARRAYSIZE(toneMappingNames));

        ImGui::SliderFloat("Exposure", &renderer_.postOptionsUBO().exposure, 0.1f, 5.0f, "%.2f");
        ImGui::SliderFloat("Gamma", &renderer_.postOptionsUBO().gamma, 1.0f / 2.2f, 2.2f, "%.2f");

        if (renderer_.postOptionsUBO().toneMappingType == 7) { // Reinhard Extended
            ImGui::SliderFloat("Max White", &renderer_.postOptionsUBO().maxWhite, 1.0f, 20.0f,
                               "%.1f");
        }
    }

    // Color Grading Controls
    if (ImGui::CollapsingHeader("Color Grading", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Contrast", &renderer_.postOptionsUBO().contrast, 0.0f, 3.0f, "%.2f");
        ImGui::SliderFloat("Brightness", &renderer_.postOptionsUBO().brightness, -1.0f, 1.0f,
                           "%.2f");
        ImGui::SliderFloat("Saturation", &renderer_.postOptionsUBO().saturation, 0.0f, 2.0f,
                           "%.2f");
        ImGui::SliderFloat("Vibrance", &renderer_.postOptionsUBO().vibrance, -1.0f, 1.0f, "%.2f");
    }

    // Effects Controls
    if (ImGui::CollapsingHeader("Effects")) {
        ImGui::SliderFloat("Vignette Strength", &renderer_.postOptionsUBO().vignetteStrength, 0.0f,
                           1.0f, "%.2f");
        if (renderer_.postOptionsUBO().vignetteStrength > 0.0f) {
            ImGui::SliderFloat("Vignette Radius", &renderer_.postOptionsUBO().vignetteRadius, 0.1f,
                               1.5f, "%.2f");
        }

        ImGui::SliderFloat("Film Grain", &renderer_.postOptionsUBO().filmGrainStrength, 0.0f, 0.2f,
                           "%.3f");
        ImGui::SliderFloat("Chromatic Aberration", &renderer_.postOptionsUBO().chromaticAberration,
                           0.0f, 5.0f, "%.1f");
    }

    // Debug Controls
    if (ImGui::CollapsingHeader("Debug Visualization")) {
        const char* debugModeNames[] = {"Off", "Tone Mapping Comparison", "Color Channels",
                                        "Split Comparison"};
        ImGui::Combo("Debug Mode", &renderer_.postOptionsUBO().debugMode, debugModeNames,
                     IM_ARRAYSIZE(debugModeNames));

        if (renderer_.postOptionsUBO().debugMode == 2) { // Color Channels
            const char* channelNames[] = {"All",       "Red Only", "Green Only",
                                          "Blue Only", "Alpha",    "Luminance"};
            ImGui::Combo("Show Channel", &renderer_.postOptionsUBO().showOnlyChannel, channelNames,
                         IM_ARRAYSIZE(channelNames));
        }

        if (renderer_.postOptionsUBO().debugMode == 3) { // Split Comparison
            ImGui::SliderFloat("Split Position", &renderer_.postOptionsUBO().debugSplit, 0.0f, 1.0f,
                               "%.2f");
        }
    }

    // Presets
    if (ImGui::CollapsingHeader("Presets")) {
        if (ImGui::Button("Default")) {
            renderer_.postOptionsUBO().toneMappingType = 2; // ACES
            renderer_.postOptionsUBO().exposure = 1.0f;
            renderer_.postOptionsUBO().gamma = 2.2f;
            renderer_.postOptionsUBO().contrast = 1.0f;
            renderer_.postOptionsUBO().brightness = 0.0f;
            renderer_.postOptionsUBO().saturation = 1.0f;
            renderer_.postOptionsUBO().vibrance = 0.0f;
            renderer_.postOptionsUBO().vignetteStrength = 0.0f;
            renderer_.postOptionsUBO().filmGrainStrength = 0.0f;
            renderer_.postOptionsUBO().chromaticAberration = 0.0f;
            renderer_.postOptionsUBO().debugMode = 0;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cinematic")) {
            renderer_.postOptionsUBO().toneMappingType = 3; // Uncharted 2
            renderer_.postOptionsUBO().exposure = 1.2f;
            renderer_.postOptionsUBO().contrast = 1.1f;
            renderer_.postOptionsUBO().saturation = 0.9f;
            renderer_.postOptionsUBO().vignetteStrength = 0.3f;
            renderer_.postOptionsUBO().vignetteRadius = 0.8f;
            renderer_.postOptionsUBO().filmGrainStrength = 0.02f;
        }

        if (ImGui::Button("High Contrast")) {
            renderer_.postOptionsUBO().contrast = 1.5f;
            renderer_.postOptionsUBO().brightness = 0.1f;
            renderer_.postOptionsUBO().saturation = 1.3f;
            renderer_.postOptionsUBO().vignetteStrength = 0.2f;
        }
        ImGui::SameLine();
        if (ImGui::Button("Low Contrast")) {
            renderer_.postOptionsUBO().contrast = 0.7f;
            renderer_.postOptionsUBO().brightness = 0.05f;
            renderer_.postOptionsUBO().saturation = 0.8f;
        }

        if (ImGui::Button("Show Tone Mapping")) {
            renderer_.postOptionsUBO().debugMode = 1;
            renderer_.postOptionsUBO().exposure = 2.0f;
        }
    }

    ImGui::End();
}

void Application::handleMouseMove(int32_t x, int32_t y)
{
    int32_t dx = (int32_t)mouseState_.position.x - x;
    int32_t dy = (int32_t)mouseState_.position.y - y;

    bool handled = false;

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

} // namespace hlab