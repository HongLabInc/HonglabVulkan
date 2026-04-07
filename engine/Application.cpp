#include "Application.h"
#include "Logger.h"
#include "GpuTimer.h"
#include "TracyProfiler.h" // Add Tracy macros wrapper

#include <format>
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <algorithm>

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
                      // {"pbrForward", {"pbrForward.vert.spv", "pbrForward.frag.spv"}},
                      {"pbrDeferred", {"pbrForward.vert.spv", "pbrDeferred.frag.spv"}},
                      {"sky", {"skybox.vert.spv", "skybox.frag.spv"}},
                      // {"ssao", {"ssao.comp.spv"}},
                      {"deferredLighting", {"deferredLighting.comp.spv"}},
                      {"post", {"post.vert.spv", "post.frag.spv"}},
                      {"gui", {"imgui.vert", "imgui.frag"}}}),
      guiRenderer_(ctx_, shaderManager_, swapchain_.colorFormat(), kMaxFramesInFlight),
      gpuTimer_(ctx_, kMaxFramesInFlight) // Initialize GPU timer
{
    initializeVulkanResources();
    setupCallbacks();
    setupCamera(config.camera);
    loadModels(config.models);

    renderer_ = std::make_unique<Renderer>(
        ctx_, shaderManager_, kMaxFramesInFlight, kAssetsPathPrefix, kShaderPathPrefix, models_,
        swapchain_.colorFormat(), ctx_.depthFormat(), windowSize_.width, windowSize_.height);

    // Initialize Tracy profiler conditionally
#ifdef TRACY_ENABLE
    tracyProfiler_ = std::make_unique<TracyProfiler>(ctx_, kMaxFramesInFlight);
    printLog("Tracy profiler initialized");
#else
    tracyProfiler_ = nullptr;
    printLog("Tracy profiler disabled (compiled without TRACY_ENABLE)");
#endif
}

// Future: Load from file constructor
Application::Application(const string& configFile)
    : Application(ApplicationConfig::createDefault()) // Fallback to default
{
    printLog("Config file loading not implemented yet, using default configuration");
}

void Application::initializeVulkanResources()
{
    commandBuffers_ = ctx_.createGraphicsCommandBuffers(kMaxFramesInFlight);

    // Initialize fences
    waitFences_.resize(kMaxFramesInFlight);
    for (auto& fence : waitFences_) {
        VkFenceCreateInfo fenceCreateInfo{};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        check(vkCreateFence(ctx_.device(), &fenceCreateInfo, nullptr, &fence));
    }

    // Acquire semaphores: per frame-in-flight (fence guards reuse)
    imageAcquiredSemaphores_.resize(kMaxFramesInFlight);
    for (size_t i = 0; i < kMaxFramesInFlight; i++) {
        VkSemaphoreCreateInfo semaphoreCI{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        check(vkCreateSemaphore(ctx_.device(), &semaphoreCI, nullptr,
                                &imageAcquiredSemaphores_[i]));
    }

    // Render-done semaphores: per swapchain image (vkAcquireNextImageKHR guards reuse)
    assert(kMaxFramesInFlight <= swapchain_.images().size()
           && "kMaxFramesInFlight must not exceed swapchain image count");
    renderDoneSemaphores_.resize(swapchain_.images().size());
    for (size_t i = 0; i < swapchain_.images().size(); i++) {
        VkSemaphoreCreateInfo semaphoreCI{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        check(vkCreateSemaphore(ctx_.device(), &semaphoreCI, nullptr,
                                &renderDoneSemaphores_[i]));
    }
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
        models_.emplace_back(std::make_unique<Model>(ctx_));
        auto& model = *models_.back();

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
            case GLFW_KEY_F4:
                // Toggle frustum culling
                {
                    bool cullingEnabled = app->renderer_->isFrustumCullingEnabled();
                    app->renderer_->setFrustumCullingEnabled(!cullingEnabled);
                    printLog("Frustum culling: {}", !cullingEnabled ? "Enabled" : "Disabled");
                }
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
                    if (model->hasAnimations()) {
                        if (model->isAnimationPlaying()) {
                            model->pauseAnimation();
                            printLog("Animation paused");
                        } else {
                            model->playAnimation();
                            printLog("Animation resumed");
                        }
                    }
                }
                break;

            case GLFW_KEY_R:
                // Restart animation
                for (auto& model : app->models_) {
                    if (model->hasAnimations()) {
                        model->stopAnimation();
                        model->playAnimation();
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
                        if (model->hasAnimations() && animIndex < model->getAnimationCount()) {
                            model->setAnimationIndex(animIndex);
                            model->playAnimation();
                            printLog("Switched to animation {}: '{}'", animIndex,
                                     model->getAnimation()->getCurrentAnimationName());
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
    ctx_.waitIdle();

    for (auto& cmd : commandBuffers_) {
        cmd.cleanup();
    }

    for (auto& sem : imageAcquiredSemaphores_) {
        vkDestroySemaphore(ctx_.device(), sem, nullptr);
    }
    for (auto& sem : renderDoneSemaphores_) {
        vkDestroySemaphore(ctx_.device(), sem, nullptr);
    }

    for (auto& fence : waitFences_) {
        vkDestroyFence(ctx_.device(), fence, nullptr);
    }

    // Destructors of members automatically cleanup everything.
}

void Application::run()
{
    TRACY_CPU_SCOPE("Application::run");

    // 파이프라인은 어떤 레이아웃으로 리소스가 들어와야 하는지는 알고 있지만
    // 구체적으로 어떤 리소스가 들어올지를 직접 결정하지는 않는다.
    // 렌더러가 파이프라인을 사용할 때 어떤 리소스를 넣을지 결정한다.

    uint32_t frameCounter = 0;
    uint32_t currentFrame = 0; // For CPU resources (command buffers, fences, acquire semaphores)

    // NEW: Animation timing variables
    auto lastTime = std::chrono::high_resolution_clock::now();
    float deltaTime = 0.016f; // Default to ~60 FPS

    while (!window_.isCloseRequested()) {
        TRACY_CPU_SCOPE("MainLoop");

        {
            TRACY_CPU_SCOPE("Window Poll Events");
            window_.pollEvents();
        }

        // NEW: Calculate delta time for smooth animation
        {
            TRACY_CPU_SCOPE("Delta Time Calculation");
            auto currentTime = std::chrono::high_resolution_clock::now();
            deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
            lastTime = currentTime;

            // Clamp delta time to prevent large jumps (e.g., when debugging)
            deltaTime = std::min(deltaTime, 0.033f); // Max 33ms (30 FPS minimum)
        }

        // Update performance metrics (both CPU FPS and GPU timing)
        {
            TRACY_CPU_SCOPE("Performance Metrics Update");
            updatePerformanceMetrics(deltaTime);
        }

        {
            TRACY_CPU_SCOPE("GUI Update");
            updateGui();
        }

        {
            TRACY_CPU_SCOPE("Camera Update");
            camera_.update(deltaTime);
            renderer_->sceneUBO().projection = camera_.matrices.perspective;
            renderer_->sceneUBO().view = camera_.matrices.view;
            renderer_->sceneUBO().cameraPos =
                glm::vec3(glm::inverse(camera_.matrices.view)[3]);
        }

        {
            TRACY_CPU_SCOPE("Animation Update");
            for (auto& model : models_) {
                if (model->hasAnimations()) {
                    model->updateAnimation(deltaTime);
                }
            }
        }

        // Update for shadow mapping
        {
            TRACY_CPU_SCOPE("Shadow Mapping Setup");

            if (models_.size() > 0) {

                glm::mat4 lightView =
                    glm::lookAt(vec3(0.0f), -renderer_->sceneUBO().directionalLightDir,
                                glm::vec3(0.0f, 0.0f, 1.0f));

                // Transform the first model's bounding box to world space
                vec3 firstMin =
                    vec3(models_[0]->modelMatrix() * vec4(models_[0]->boundingBoxMin(), 1.0f));
                vec3 firstMax =
                    vec3(models_[0]->modelMatrix() * vec4(models_[0]->boundingBoxMax(), 1.0f));

                // Ensure min is actually smaller than max for each component
                vec3 min_ = glm::min(firstMin, firstMax);
                vec3 max_ = glm::max(firstMin, firstMax);

                // Iterate through all remaining models to determine the combined bounding box
                for (uint32_t i = 1; i < models_.size(); i++) {
                    // Transform this model's bounding box to world space
                    vec3 modelMin =
                        vec3(models_[i]->modelMatrix() * vec4(models_[i]->boundingBoxMin(), 1.0f));
                    vec3 modelMax =
                        vec3(models_[i]->modelMatrix() * vec4(models_[i]->boundingBoxMax(), 1.0f));

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
                renderer_->sceneUBO().lightSpaceMatrix = lightProjection * lightView;

                // Modifed "Vulkan 3D Graphics Rendering Cookbook - 2nd Edition Build Status"
                // https://github.com/PacktPublishing/3D-Graphics-Rendering-Cookbook-Second-Edition
            }
        }

        // Wait using currentFrame index (CPU-side fence)
        {
            TRACY_CPU_SCOPE("Fence Wait");
            check(vkWaitForFences(ctx_.device(), 1, &waitFences_[currentFrame], VK_TRUE, UINT64_MAX));
            check(vkResetFences(ctx_.device(), 1, &waitFences_[currentFrame]));
        }

        {
            TRACY_CPU_SCOPE("Renderer Update");
            renderer_->update(camera_, models_, currentFrame, (float)glfwGetTime());
        }

        {
            TRACY_CPU_SCOPE("GUI Renderer Update");
            guiRenderer_.update(currentFrame);
        }

        // Acquire using currentFrame index (fence guards semaphore reuse)
        uint32_t imageIndex{0};
        VkResult result;
        {
            TRACY_CPU_SCOPE("Swapchain Image Acquire");
            result = vkAcquireNextImageKHR(ctx_.device(), swapchain_.handle(), UINT64_MAX,
                                           imageAcquiredSemaphores_[currentFrame],
                                           VK_NULL_HANDLE, &imageIndex);
        }

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            continue; // Ignore resize in this example
        } else if ((result != VK_SUCCESS) && (result != VK_SUBOPTIMAL_KHR)) {
            exitWithMessage("Could not acquire the next swap chain image!");
        }

        // Use currentFrame index (CPU-side command buffer)
        CommandBuffer& cmd = commandBuffers_[currentFrame];

        // Begin command buffer
        {
            TRACY_CPU_SCOPE("Command Buffer Begin");
            vkResetCommandBuffer(cmd.handle(), 0);
            VkCommandBufferBeginInfo cmdBufferBeginInfo{
                VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            check(vkBeginCommandBuffer(cmd.handle(), &cmdBufferBeginInfo));
        }

        // *** GPU TIMING START ***
        // Reset and begin GPU timing for this frame
        {
            TRACY_CPU_SCOPE("GPU Timer Setup");
            gpuTimer_.resetQueries(cmd.handle(), currentFrame);
            gpuTimer_.beginFrame(cmd.handle(), currentFrame);
        }

        // *** TRACY GPU PROFILING START ***
        if (tracyProfiler_) {
            TRACY_CPU_SCOPE("Tracy GPU Setup");
            tracyProfiler_->beginFrame(cmd.handle(), currentFrame);
        }

        // Transition swapchain image from undefined to color attachment layout
        {
            TRACY_CPU_SCOPE("Swapchain Barrier Setup");
            if (tracyProfiler_) {
                TRACY_GPU_SCOPE(*tracyProfiler_, cmd.handle(), "Swapchain Transition");
            }
            swapchain_.barrierHelper(imageIndex)
                .transitionTo(cmd.handle(), VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                              VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
        }

        VkViewport viewport{0.0f, 0.0f, (float)windowSize_.width, (float)windowSize_.height,
                            0.0f, 1.0f};
        VkRect2D scissor{0, 0, windowSize_.width, windowSize_.height};

        // Draw models
        {
            TRACY_CPU_SCOPE("Renderer Draw Call");
            if (tracyProfiler_) {
                TRACY_GPU_SCOPE(*tracyProfiler_, cmd.handle(), "Rendering");
            }
            renderer_->draw(cmd.handle(), currentFrame, swapchain_.imageView(imageIndex), models_,
                            viewport, scissor);
        }

        // Draw GUI (overwrite to swapchain image)
        {
            TRACY_CPU_SCOPE("GUI Draw Call");
            if (tracyProfiler_) {
                TRACY_GPU_SCOPE(*tracyProfiler_, cmd.handle(), "GUI Rendering");
            }
            guiRenderer_.draw(cmd.handle(), swapchain_.imageView(imageIndex), viewport,
                              currentFrame);
        }

        {
            TRACY_CPU_SCOPE("Swapchain Present Barrier");
            if (tracyProfiler_) {
                TRACY_GPU_SCOPE(*tracyProfiler_, cmd.handle(), "Swapchain Present Transition");
            }
            swapchain_.barrierHelper(imageIndex)
                .transitionTo(cmd.handle(), VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                              VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);
        }

        // *** GPU TIMING END ***
        // End GPU timing (excludes presentation)
        {
            TRACY_CPU_SCOPE("GPU Timer End");
            gpuTimer_.endFrame(cmd.handle(), currentFrame);
        }

        {
            TRACY_CPU_SCOPE("Command Buffer End");
            check(vkEndCommandBuffer(cmd.handle())); // End command buffer
        }

        VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        // 비교: 마지막으로 실행되는 셰이더가 Compute라면 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT

        VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submitInfo.pCommandBuffers = &cmd.handle();
        submitInfo.commandBufferCount = 1;
        submitInfo.pWaitDstStageMask = &waitStageMask;
        submitInfo.pWaitSemaphores = &imageAcquiredSemaphores_[currentFrame];
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &renderDoneSemaphores_[imageIndex];
        submitInfo.signalSemaphoreCount = 1;

        {
            TRACY_CPU_SCOPE("GPU Submit");
            check(vkQueueSubmit(cmd.queue(), 1, &submitInfo, waitFences_[currentFrame]));
        }

        // Present (NOT included in GPU timing)
        VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &renderDoneSemaphores_[imageIndex];
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchain_.handle();
        presentInfo.pImageIndices = &imageIndex;

        {
            TRACY_CPU_SCOPE("Present");
            check(vkQueuePresentKHR(ctx_.graphicsQueue(), &presentInfo));
        }

        currentFrame = (currentFrame + 1) % kMaxFramesInFlight;

        frameCounter++;

        // *** TRACY FRAME MARK ***
        if (tracyProfiler_) {
            tracyProfiler_->endFrame();
        }

        // Track performance data in Tracy
        if (tracyProfiler_ && tracyProfiler_->isTracySupported()) {
            tracyProfiler_->plot("CPU_FPS", currentFPS_);
            tracyProfiler_->plot("GPU_Time_ms", currentGpuTimeMs_);
            tracyProfiler_->plot("Frame_Delta_ms", deltaTime * 1000.0f);

            if (frameCounter % 60 == 0) { // Every 60 frames
                char message[128];
                snprintf(message, sizeof(message), "Frame %u - FPS: %.1f, GPU: %.2fms",
                         frameCounter, currentFPS_, currentGpuTimeMs_);
                tracyProfiler_->messageL(message);
            }
        }

        // Mark frame end for Tracy
        FrameMark;
    }

    {
        TRACY_CPU_SCOPE("Application Shutdown");
        ctx_.waitIdle(); // 종료하기 전 GPU 사용이 모두 끝날때까지 대기
    }
}

void Application::updateGui()
{
    TRACY_CPU_SCOPE("Application::updateGui");

    static float scale = 1.4f;

    ImGuiIO& io = ImGui::GetIO();

    io.DisplaySize = ImVec2(float(windowSize_.width), float(windowSize_.height));
    // io.DeltaTime = frameTimer;

    // Always pass mouse input to ImGui - let ImGui decide if it wants to capture it
    io.MousePos = ImVec2(mouseState_.position.x, mouseState_.position.y);
    io.MouseDown[0] = mouseState_.buttons.left;
    io.MouseDown[1] = mouseState_.buttons.right;
    io.MouseDown[2] = mouseState_.buttons.middle;

    {
        TRACY_CPU_SCOPE("ImGui NewFrame");
        ImGui::NewFrame();
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::SetNextWindowPos(ImVec2(10 * scale, 10 * scale), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiCond_FirstUseEver);
    ImGui::Begin("벌컨 실시간 렌더링 예제", nullptr, ImGuiWindowFlags_None);

    // Enhanced performance display
    ImGui::Text("CPU FPS: %.1f (%.2f ms/frame)", currentFPS_,
                1000.0f / std::max(currentFPS_, 1.0f));

    if (gpuTimer_.isTimestampSupported()) {
        ImGui::Text("GPU Time: %.2f ms", currentGpuTimeMs_);

        // Calculate GPU FPS equivalent for comparison
        float gpuFPS = currentGpuTimeMs_ > 0.0f ? 1000.0f / currentGpuTimeMs_ : 0.0f;
        ImGui::Text("GPU FPS equiv: %.1f", gpuFPS);

        // Debug information
        ImGui::Text("Debug: Results ready: %s", gpuTimer_.hasAnyResultsReady() ? "Yes" : "No");
    } else {
        ImGui::Text("GPU Time: Not supported");
    }

    // Tracy profiler status
    if (tracyProfiler_ && tracyProfiler_->isTracySupported()) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Tracy Profiler Active");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Tracy profiler is connected and collecting data.\n"
                              "Connect Tracy client to view detailed profiling information.");
        }
    } else {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "○ Tracy Profiler Disabled");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Tracy profiler is not available.\n"
                              "Compile with -DTRACY_ENABLE to enable profiling.");
        }
    }

    // Color-coded performance indicators
    ImVec4 cpuColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); // Green for good FPS
    if (currentFPS_ < 30.0f) {
        cpuColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Red
    } else if (currentFPS_ < 60.0f) {
        cpuColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); // Yellow
    }

    ImVec4 gpuColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); // Green for good GPU time
    if (currentGpuTimeMs_ > 33.33f) {                 // > 30 FPS equivalent
        gpuColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);    // Red
    } else if (currentGpuTimeMs_ > 16.67f) {          // > 60 FPS equivalent
        gpuColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);    // Yellow
    }

    ImGui::SameLine();
    ImGui::TextColored(cpuColor, "● CPU");
    if (gpuTimer_.isTimestampSupported()) {
        ImGui::SameLine();
        ImGui::TextColored(gpuColor, "● GPU");
    }
    if (tracyProfiler_ && tracyProfiler_->isTracySupported()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "● Tracy");
    }

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Performance Indicators\n"
                          "CPU: Frame rate (includes CPU overhead)\n"
                          "GPU: Pure GPU rendering time (excludes presentation)\n"
                          "Tracy: Real-time profiler (connect Tracy client for details)\n"
                          "Green: Good performance\n"
                          "Yellow: Moderate performance\n"
                          "Red: Poor performance");
    }

    ImGui::Separator();

    static vec3 lightColor = vec3(1.0f);
    static float lightIntensity = 28.454f;
    ImGui::SliderFloat("Light Intensity", &lightIntensity, 0.0f, 100.0f);
    renderer_->sceneUBO().directionalLightColor = lightIntensity * lightColor;

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
    renderer_->sceneUBO().directionalLightDir = lightDir;

    // Display current light direction for debugging
    ImGui::Text("Light Dir: (%.2f, %.2f, %.2f)", renderer_->sceneUBO().directionalLightDir.x,
                renderer_->sceneUBO().directionalLightDir.y,
                renderer_->sceneUBO().directionalLightDir.z);

    // Rendering Options Controls
    ImGui::Separator();
    ImGui::Text("Rendering Options");

    bool textureOn = renderer_->optionsUBO().textureOn != 0;
    bool shadowOn = renderer_->optionsUBO().shadowOn != 0;
    bool discardOn = renderer_->optionsUBO().discardOn != 0;
    // bool animationOn = renderer_->optionsUBO().animationOn != 0;

    if (ImGui::Checkbox("Textures", &textureOn)) {
        renderer_->optionsUBO().textureOn = textureOn ? 1 : 0;
    }
    if (ImGui::Checkbox("Shadows", &shadowOn)) {
        renderer_->optionsUBO().shadowOn = shadowOn ? 1 : 0;
    }
    if (ImGui::Checkbox("Alpha Discard", &discardOn)) {
        renderer_->optionsUBO().discardOn = discardOn ? 1 : 0;
    }
    // if (ImGui::Checkbox("Animation", &animationOn)) {
    //     renderer_.optionsUBO().animationOn = animationOn ? 1 : 0;
    // }

    // PBR Lighting Controls for Deferred Rendering
    ImGui::Separator();
    ImGui::Text("PBR Lighting (Global)");

    ImGui::SliderFloat("Specular Weight", &renderer_->optionsUBO().specularWeight, 0.0f, 0.1f,
                       "%.3f");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Controls global specular reflection intensity.\n"
                          "Higher values = stronger reflections");
    }

    ImGui::SliderFloat("Diffuse Weight", &renderer_->optionsUBO().diffuseWeight, 0.0f, 2.0f,
                       "%.2f");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Controls global diffuse lighting intensity.\n"
                          "Higher values = brighter base lighting");
    }

    ImGui::SliderFloat("Emissive Weight", &renderer_->optionsUBO().emissiveWeight, 0.0f, 5.0f,
                       "%.2f");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Controls global emissive glow intensity.\n"
                          "Higher values = stronger self-illumination");
    }

    ImGui::SliderFloat("Shadow Offset", &renderer_->optionsUBO().shadowOffset, -0.1f, 0.1f, "%.3f");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Global shadow bias offset.\n"
                          "Positive = lighter shadows\n"
                          "Negative = darker shadows");
    }

    // Quick presets for PBR lighting
    ImGui::Text("PBR Presets:");
    if (ImGui::Button("Default")) {
        renderer_->optionsUBO().specularWeight = 0.05f;
        renderer_->optionsUBO().diffuseWeight = 1.0f;
        renderer_->optionsUBO().emissiveWeight = 1.0f;
        renderer_->optionsUBO().shadowOffset = 0.0f;
    }
    ImGui::SameLine();
    if (ImGui::Button("Bright")) {
        renderer_->optionsUBO().specularWeight = 0.08f;
        renderer_->optionsUBO().diffuseWeight = 1.3f;
        renderer_->optionsUBO().emissiveWeight = 1.5f;
        renderer_->optionsUBO().shadowOffset = 0.02f;
    }

    if (ImGui::Button("Matte")) {
        renderer_->optionsUBO().specularWeight = 0.02f;
        renderer_->optionsUBO().diffuseWeight = 1.5f;
        renderer_->optionsUBO().emissiveWeight = 0.8f;
        renderer_->optionsUBO().shadowOffset = 0.0f;
    }
    ImGui::SameLine();
    if (ImGui::Button("Metallic")) {
        renderer_->optionsUBO().specularWeight = 0.12f;
        renderer_->optionsUBO().diffuseWeight = 0.7f;
        renderer_->optionsUBO().emissiveWeight = 1.0f;
        renderer_->optionsUBO().shadowOffset = 0.01f;
    }

    // Frustum Culling Controls
    bool frustumCullingEnabled = renderer_->isFrustumCullingEnabled();
    if (ImGui::Checkbox("Frustum Culling", &frustumCullingEnabled)) {
        renderer_->setFrustumCullingEnabled(frustumCullingEnabled);
    }

    // Display culling statistics
    if (renderer_->isFrustumCullingEnabled()) {
        const auto& stats = renderer_->getCullingStats();
        ImGui::Text("Culling Stats:");
        ImGui::Text("  Total Meshes: %u", stats.totalMeshes);
        ImGui::Text("  Rendered: %u", stats.renderedMeshes);
        ImGui::Text("  Culled: %u", stats.culledMeshes);

        if (stats.totalMeshes > 0) {
            float cullingPercentage =
                (float(stats.culledMeshes) / float(stats.totalMeshes)) * 100.0f;
            ImGui::Text("  Culled: %.1f%%", cullingPercentage);
        }
    }

    ImGui::Separator();

    for (uint32_t i = 0; i < models_.size(); i++) {
        auto& m = *models_[i];
        ImGui::Checkbox(std::format("{}##{}", m.name(), i).c_str(), &m.visible());

        // clean
        ImGui::SliderFloat(format("SpecularWeight##{}", i).c_str(), &(m.coeffs()[0]), 0.0f, 1.0f);
        ImGui::SliderFloat(format("DiffuseWeight##{}", i).c_str(), &(m.coeffs()[1]), 0.0f, 10.0f);
        ImGui::SliderFloat(format("EmissiveWeight##{}", i).c_str(), &(m.coeffs()[2]), 0.0f, 10.0f);
        ImGui::SliderFloat(format("ShadowOffset##{}", i).c_str(), &(m.coeffs()[3]), 0.0f, 1.0f);
        ImGui::SliderFloat(format("MetallicWeight##{}", i).c_str(), &(m.coeffs()[4]), 0.0f, 1.0f);
        ImGui::SliderFloat(format("RoughnessWeight##{}", i).c_str(), &(m.coeffs()[5]), 0.0f, 1.0f);

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

    // Camera Control Window
    renderCameraControlWindow();

    renderHDRControlWindow();

    renderPostProcessingControlWindow();

    // Add this line:
    renderSSAOControlWindow();

    {
        TRACY_CPU_SCOPE("ImGui Render");
        ImGui::Render();
    }
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
        ImGui::SliderFloat("Environment Intensity",
                           &renderer_->skyOptionsUBO().environmentIntensity, 0.0f, 10.0f, "%.2f");
    }

    // Environment Map Controls
    if (ImGui::CollapsingHeader("Environment Map", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Roughness Level", &renderer_->skyOptionsUBO().roughnessLevel, 0.0f,
                           8.0f, "%.1f");

        bool useIrradiance = renderer_->skyOptionsUBO().useIrradianceMap != 0;
        if (ImGui::Checkbox("Use Irradiance Map", &useIrradiance)) {
            renderer_->skyOptionsUBO().useIrradianceMap = useIrradiance ? 1 : 0;
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
        bool showMipLevels = renderer_->skyOptionsUBO().showMipLevels != 0;
        if (ImGui::Checkbox("Show Mip Levels", &showMipLevels)) {
            renderer_->skyOptionsUBO().showMipLevels = showMipLevels ? 1 : 0;
        }

        bool showCubeFaces = renderer_->skyOptionsUBO().showCubeFaces != 0;
        if (ImGui::Checkbox("Show Cube Faces", &showCubeFaces)) {
            renderer_->skyOptionsUBO().showCubeFaces = showCubeFaces ? 1 : 0;
        }
    }

    // Simplified Presets
    if (ImGui::CollapsingHeader("Presets")) {
        if (ImGui::Button("Default")) {
            renderer_->skyOptionsUBO().environmentIntensity = 1.0f;
            renderer_->skyOptionsUBO().roughnessLevel = 0.5f;
            renderer_->skyOptionsUBO().useIrradianceMap = 0;
            renderer_->skyOptionsUBO().showMipLevels = 0;
            renderer_->skyOptionsUBO().showCubeFaces = 0;
        }
        ImGui::SameLine();
        if (ImGui::Button("High Exposure")) {
            renderer_->skyOptionsUBO().environmentIntensity = 1.5f;
        }
        ImGui::SameLine();
        if (ImGui::Button("Low Exposure")) {
            renderer_->skyOptionsUBO().environmentIntensity = 0.8f;
        }

        if (ImGui::Button("Sharp Reflections")) {
            renderer_->skyOptionsUBO().roughnessLevel = 0.0f;
            renderer_->skyOptionsUBO().useIrradianceMap = 0;
        }
        ImGui::SameLine();
        if (ImGui::Button("Diffuse Lighting")) {
            renderer_->skyOptionsUBO().useIrradianceMap = 1;
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
        ImGui::Combo("Tone Mapping Type", &renderer_->postOptionsUBO().toneMappingType,
                     toneMappingNames, IM_ARRAYSIZE(toneMappingNames));

        ImGui::SliderFloat("Exposure", &renderer_->postOptionsUBO().exposure, 0.1f, 5.0f, "%.2f");
        ImGui::SliderFloat("Gamma", &renderer_->postOptionsUBO().gamma, 1.0f / 2.2f, 2.2f, "%.2f");

        if (renderer_->postOptionsUBO().toneMappingType == 7) { // Reinhard Extended
            ImGui::SliderFloat("Max White", &renderer_->postOptionsUBO().maxWhite, 1.0f, 20.0f,
                               "%.1f");
        }
    }

    // Color Grading Controls
    if (ImGui::CollapsingHeader("Color Grading", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Contrast", &renderer_->postOptionsUBO().contrast, 0.0f, 3.0f, "%.2f");
        ImGui::SliderFloat("Brightness", &renderer_->postOptionsUBO().brightness, -1.0f, 1.0f,
                           "%.2f");
        ImGui::SliderFloat("Saturation", &renderer_->postOptionsUBO().saturation, 0.0f, 2.0f,
                           "%.2f");
        ImGui::SliderFloat("Vibrance", &renderer_->postOptionsUBO().vibrance, -1.0f, 1.0f, "%.2f");
    }

    // Effects Controls
    if (ImGui::CollapsingHeader("Effects")) {
        ImGui::SliderFloat("Vignette Strength", &renderer_->postOptionsUBO().vignetteStrength, 0.0f,
                           1.0f, "%.2f");
        if (renderer_->postOptionsUBO().vignetteStrength > 0.0f) {
            ImGui::SliderFloat("Vignette Radius", &renderer_->postOptionsUBO().vignetteRadius, 0.1f,
                               1.5f, "%.2f");
        }

        ImGui::SliderFloat("Film Grain", &renderer_->postOptionsUBO().filmGrainStrength, 0.0f, 0.2f,
                           "%.3f");

        // Anti-aliasing and Chromatic Aberration Controls (dual-purpose parameter)
        ImGui::Separator();
        ImGui::Text("Anti-Aliasing / Chromatic Aberration:");

        float& chromAberr = renderer_->postOptionsUBO().chromaticAberration;

        // Determine current mode
        bool fxaaEnabled = chromAberr > 1.0f;
        bool chromEnabled = chromAberr > 0.0f && chromAberr <= 1.0f;

        // Mode selection
        if (ImGui::RadioButton("Off", !fxaaEnabled && !chromEnabled)) {
            chromAberr = 0.0f;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("FXAA", fxaaEnabled)) {
            chromAberr = fxaaEnabled ? chromAberr : 1.5f; // Default FXAA strength
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Chromatic Aberration", chromEnabled)) {
            chromAberr = chromEnabled ? chromAberr : 0.5f; // Default chrom aberr strength
        }

        // Controls based on current mode
        if (fxaaEnabled) {
            float fxaaStrength = chromAberr - 1.0f;
            if (ImGui::SliderFloat("FXAA Strength", &fxaaStrength, 0.1f, 1.0f, "%.2f")) {
                chromAberr = 1.0f + fxaaStrength;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("FXAA Anti-Aliasing Strength\n"
                                  "0.1 = Light smoothing, good performance\n"
                                  "0.5 = Balanced quality and performance\n"
                                  "1.0 = Maximum smoothing, lower performance");
            }

            // Advanced Quality Control
            ImGui::Separator();
            ImGui::Text("Advanced FXAA Quality:");

            // Extract quality level from fractional part
            float baseStrength = std::floor(fxaaStrength * 10.0f) / 10.0f;
            float qualityLevel = (fxaaStrength - baseStrength) * 10.0f;

            // Quality presets with encoded settings
            if (ImGui::Button("Fast##fxaa")) {
                chromAberr = 1.25f; // 0.25 strength, 0.0 quality (4 samples)
            }
            ImGui::SameLine();
            if (ImGui::Button("Balanced##fxaa")) {
                chromAberr = 1.55f; // 0.5 strength, 0.5 quality (8 samples)
            }
            ImGui::SameLine();
            if (ImGui::Button("Quality##fxaa")) {
                chromAberr = 1.79f; // 0.7 strength, 0.9 quality (12 samples)
            }

            // Quality level slider (affects sample count and edge detection)
            float newQualityLevel = qualityLevel;
            if (ImGui::SliderFloat("Sample Quality", &newQualityLevel, 0.0f, 1.0f, "%.2f")) {
                chromAberr = 1.0f + baseStrength + (newQualityLevel * 0.1f);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Controls FXAA sample count and edge detection quality\n"
                                  "0.0 = 4 samples, basic edge detection\n"
                                  "0.5 = 8 samples, enhanced edge detection\n"
                                  "1.0 = 12 samples, premium edge detection");
            }

            // Real-time quality information
            int estimatedSamples = int(4.0f + qualityLevel * 8.0f);
            bool extendedSampling = qualityLevel > 0.5f;
            ImGui::Text("Current Settings:");
            ImGui::BulletText("Sample Count: %d", estimatedSamples);
            ImGui::BulletText("Edge Detection: %s", extendedSampling ? "Enhanced" : "Basic");
            ImGui::BulletText("Performance Cost: ~%.1f%%", (2.0f + qualityLevel * 3.0f));

        } else if (chromEnabled) {
            if (ImGui::SliderFloat("Aberration Strength", &chromAberr, 0.0f, 1.0f, "%.3f")) {
                // Ensure we stay in chromatic aberration range
                chromAberr = std::clamp(chromAberr, 0.0f, 1.0f);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Chromatic Aberration Effect\n"
                                  "Simulates lens distortion where colors separate\n"
                                  "Higher values = more dramatic color fringing");
            }
        }

        // Info text
        if (fxaaEnabled) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ FXAA Active");
            ImGui::Text("Performance impact: ~2-5%%");
        } else if (chromEnabled) {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "✓ Chromatic Aberration Active");
            ImGui::Text("Performance impact: ~1-2%%");
        } else {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "○ No Effect Active");
        }

        // Bokeh Depth of Field Controls
        ImGui::Separator();
        ImGui::Text("Bokeh Depth of Field:");

        float& padding1 = renderer_->postOptionsUBO().padding1;

        // Decode current Bokeh parameters
        float focusDistance = std::floor(padding1 / 10000.0f) / 100.0f;
        float aperture = std::floor(std::fmod(padding1, 10000.0f) / 100.0f) / 100.0f;
        float intensity = std::fmod(padding1, 100.0f) / 100.0f;

        bool bokehEnabled = intensity > 0.0f;
        if (ImGui::Checkbox("Enable Bokeh DOF", &bokehEnabled)) {
            if (!bokehEnabled) {
                intensity = 0.0f;
            } else if (intensity == 0.0f) {
                intensity = 0.5f;     // Default intensity
                focusDistance = 0.3f; // Default focus distance
                aperture = 0.3f;      // Default aperture
            }
        }

        if (bokehEnabled) {
            // Focus Distance Control
            if (ImGui::SliderFloat("Focus Distance", &focusDistance, 0.0f, 1.0f, "%.2f")) {
                // Re-encode parameters
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Distance to the focal plane\n"
                                  "0.0 = Near focus (foreground sharp)\n"
                                  "0.5 = Middle focus\n"
                                  "1.0 = Far focus (background sharp)");
            }

            // Aperture Control
            if (ImGui::SliderFloat("Aperture Size", &aperture, 0.0f, 1.0f, "%.2f")) {
                // Re-encode parameters
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Controls the size of the blur circles\n"
                                  "0.0 = Small aperture (sharp)\n"
                                  "0.5 = Medium aperture\n"
                                  "1.0 = Large aperture (very blurry)");
            }

            // Bokeh Intensity Control
            if (ImGui::SliderFloat("Bokeh Intensity", &intensity, 0.1f, 1.0f, "%.2f")) {
                // Re-encode parameters
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Controls the strength of the Bokeh effect\n"
                                  "0.1 = Subtle depth of field\n"
                                  "0.5 = Moderate effect\n"
                                  "1.0 = Strong cinematic Bokeh");
            }

            // Re-encode parameters into padding1
            padding1 = std::floor(focusDistance * 100.0f) * 10000.0f +
                       std::floor(aperture * 100.0f) * 100.0f + std::floor(intensity * 100.0f);

            // Bokeh Quality Presets
            ImGui::Text("Bokeh Presets:");
            if (ImGui::Button("Portrait##bokeh")) {
                focusDistance = 0.2f; // Close focus
                aperture = 0.7f;      // Wide aperture
                intensity = 0.8f;     // Strong effect
            }
            ImGui::SameLine();
            if (ImGui::Button("Landscape##bokeh")) {
                focusDistance = 0.6f; // Far focus
                aperture = 0.3f;      // Small aperture
                intensity = 0.4f;     // Subtle effect
            }
            ImGui::SameLine();
            if (ImGui::Button("Macro##bokeh")) {
                focusDistance = 0.1f; // Very close focus
                aperture = 0.9f;      // Very wide aperture
                intensity = 1.0f;     // Maximum effect
            }

            // Real-time information
            ImGui::Text("Bokeh Status:");
            ImGui::BulletText("Focus: %.1fm", focusDistance * 50.0f + 0.1f);
            ImGui::BulletText("f-stop: f/%.1f", 1.0f / (aperture * 0.1f + 0.001f));
            ImGui::BulletText("Max blur: %.0fpx", aperture * intensity * 20.0f);

            // Performance warning
            if (intensity > 0.7f && aperture > 0.7f) {
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "⚠ High performance cost");
            }
        }
    }

    // Debug Controls
    if (ImGui::CollapsingHeader("Debug Visualization")) {
        const char* debugModeNames[] = {"Off", "Tone Mapping Comparison", "Color Channels",
                                        "Split Comparison", "Bokeh Depth Visualization"};
        ImGui::Combo("Debug Mode", &renderer_->postOptionsUBO().debugMode, debugModeNames,
                     IM_ARRAYSIZE(debugModeNames));

        if (renderer_->postOptionsUBO().debugMode == 2) { // Color Channels
            const char* channelNames[] = {"All",       "Red Only", "Green Only",
                                          "Blue Only", "Alpha",    "Luminance"};
            ImGui::Combo("Show Channel", &renderer_->postOptionsUBO().showOnlyChannel, channelNames,
                         IM_ARRAYSIZE(channelNames));
        }

        if (renderer_->postOptionsUBO().debugMode == 3) { // Split Comparison
            ImGui::SliderFloat("Split Position", &renderer_->postOptionsUBO().debugSplit, 0.0f,
                               1.0f, "%.2f");
        }

        if (renderer_->postOptionsUBO().debugMode == 4) { // Bokeh Depth Visualization
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Green: Sharp areas");
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Yellow: Moderate blur");
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Red: Maximum blur");
        }
    }

    // Presets
    if (ImGui::CollapsingHeader("Presets")) {
        if (ImGui::Button("Default")) {
            renderer_->postOptionsUBO().toneMappingType = 2; // ACES
            renderer_->postOptionsUBO().exposure = 1.0f;
            renderer_->postOptionsUBO().gamma = 2.2f;
            renderer_->postOptionsUBO().contrast = 1.0f;
            renderer_->postOptionsUBO().brightness = 0.0f;
            renderer_->postOptionsUBO().saturation = 1.0f;
            renderer_->postOptionsUBO().vibrance = 0.0f;
            renderer_->postOptionsUBO().vignetteStrength = 0.0f;
            renderer_->postOptionsUBO().filmGrainStrength = 0.0f;
            renderer_->postOptionsUBO().chromaticAberration = 0.0f;
            renderer_->postOptionsUBO().debugMode = 0;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cinematic")) {
            renderer_->postOptionsUBO().toneMappingType = 3; // Uncharted 2
            renderer_->postOptionsUBO().exposure = 1.2f;
            renderer_->postOptionsUBO().contrast = 1.1f;
            renderer_->postOptionsUBO().saturation = 0.9f;
            renderer_->postOptionsUBO().vignetteStrength = 0.3f;
            renderer_->postOptionsUBO().vignetteRadius = 0.8f;
            renderer_->postOptionsUBO().filmGrainStrength = 0.02f;
            renderer_->postOptionsUBO().chromaticAberration = 0.2f; // Light chromatic aberration
        }

        if (ImGui::Button("High Quality + FXAA")) {
            renderer_->postOptionsUBO().toneMappingType = 2; // ACES
            renderer_->postOptionsUBO().exposure = 1.1f;
            renderer_->postOptionsUBO().contrast = 1.05f;
            renderer_->postOptionsUBO().saturation = 1.1f;
            renderer_->postOptionsUBO().vignetteStrength = 0.1f;
            renderer_->postOptionsUBO().filmGrainStrength = 0.0f;
            renderer_->postOptionsUBO().chromaticAberration =
                1.79f; // High quality FXAA (0.7 strength, 0.9 quality)
        }
        ImGui::SameLine();
        if (ImGui::Button("Performance + FXAA")) {
            renderer_->postOptionsUBO().toneMappingType = 1; // Reinhard (faster)
            renderer_->postOptionsUBO().exposure = 1.0f;
            renderer_->postOptionsUBO().contrast = 1.0f;
            renderer_->postOptionsUBO().saturation = 1.0f;
            renderer_->postOptionsUBO().vignetteStrength = 0.0f;
            renderer_->postOptionsUBO().filmGrainStrength = 0.0f;
            renderer_->postOptionsUBO().chromaticAberration =
                1.25f; // Fast FXAA (0.25 strength, 0.0 quality)
        }

        if (ImGui::Button("Show Tone Mapping")) {
            renderer_->postOptionsUBO().debugMode = 1;
            renderer_->postOptionsUBO().exposure = 2.0f;
            renderer_->postOptionsUBO().chromaticAberration = 0.0f; // Disable effects for debug
        }
        ImGui::SameLine();
        if (ImGui::Button("Show FXAA Effect")) {
            renderer_->postOptionsUBO().debugMode = 3; // Split comparison
            renderer_->postOptionsUBO().debugSplit = 0.5f;
            renderer_->postOptionsUBO().chromaticAberration =
                1.89f; // Maximum quality FXAA (0.8 strength, 0.9 quality)
        }
        ImGui::SameLine();
        if (ImGui::Button("Show Bokeh Depth")) {
            renderer_->postOptionsUBO().debugMode = 4; // Bokeh depth visualization
            // Enable Bokeh with moderate settings for visualization
            renderer_->postOptionsUBO().padding1 =
                30.0f * 10000.0f + 50.0f * 100.0f + 50.0f; // Focus=0.3, Aperture=0.5, Intensity=0.5
        }

        // Additional FXAA-specific presets
        if (ImGui::Button("Ultra FXAA")) {
            renderer_->postOptionsUBO().toneMappingType = 2; // ACES
            renderer_->postOptionsUBO().exposure = 1.0f;
            renderer_->postOptionsUBO().contrast = 1.0f;
            renderer_->postOptionsUBO().saturation = 1.0f;
            renderer_->postOptionsUBO().vignetteStrength = 0.0f;
            renderer_->postOptionsUBO().filmGrainStrength = 0.0f;
            renderer_->postOptionsUBO().chromaticAberration =
                1.99f;                                   // Maximum FXAA (0.9 strength, 0.9 quality)
            renderer_->postOptionsUBO().padding1 = 0.0f; // Disable Bokeh
        }
        ImGui::SameLine();
        if (ImGui::Button("Cinematic Bokeh")) {
            renderer_->postOptionsUBO().toneMappingType = 3; // Uncharted 2
            renderer_->postOptionsUBO().exposure = 1.1f;
            renderer_->postOptionsUBO().contrast = 1.05f;
            renderer_->postOptionsUBO().saturation = 0.95f;
            renderer_->postOptionsUBO().vignetteStrength = 0.2f;
            renderer_->postOptionsUBO().filmGrainStrength = 0.01f;
            renderer_->postOptionsUBO().chromaticAberration = 0.1f; // Light chromatic aberration
            renderer_->postOptionsUBO().padding1 =
                25.0f * 10000.0f + 70.0f * 100.0f + 75.0f; // Cinematic Bokeh settings
        }

        if (ImGui::Button("Photo Realism")) {
            renderer_->postOptionsUBO().toneMappingType = 2; // ACES
            renderer_->postOptionsUBO().exposure = 1.0f;
            renderer_->postOptionsUBO().contrast = 1.02f;
            renderer_->postOptionsUBO().saturation = 1.05f;
            renderer_->postOptionsUBO().vignetteStrength = 0.1f;
            renderer_->postOptionsUBO().filmGrainStrength = 0.005f;
            renderer_->postOptionsUBO().chromaticAberration = 1.5f; // Medium FXAA
            renderer_->postOptionsUBO().padding1 =
                40.0f * 10000.0f + 40.0f * 100.0f + 60.0f; // Realistic Bokeh
        }
    }

    ImGui::End();
}

// NEW: Camera Control window method
void Application::renderCameraControlWindow()
{
    ImGui::SetNextWindowPos(ImVec2(10, 350), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Camera Controls")) {
        ImGui::End();
        return;
    }

    // Camera Information Display
    if (ImGui::CollapsingHeader("Camera Information", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Position: (%.2f, %.2f, %.2f)", camera_.position.x, camera_.position.y,
                    camera_.position.z);
        ImGui::Text("Rotation: (%.2f°, %.2f°, %.2f°)", camera_.rotation.x, camera_.rotation.y,
                    camera_.rotation.z);
        ImGui::Text("View Pos: (%.2f, %.2f, %.2f)", camera_.viewPos.x, camera_.viewPos.y,
                    camera_.viewPos.z);

        // Camera Type Toggle
        bool isFirstPerson = camera_.type == hlab::Camera::CameraType::firstperson;
        if (ImGui::Checkbox("First Person Mode", &isFirstPerson)) {
            camera_.type = isFirstPerson ? hlab::Camera::CameraType::firstperson
                                         : hlab::Camera::CameraType::lookat;
        }
    }

    // Camera Position Controls
    if (ImGui::CollapsingHeader("Position Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
        glm::vec3 tempPosition = camera_.position;
        if (ImGui::SliderFloat3("Position", &tempPosition.x, -50.0f, 50.0f, "%.2f")) {
            camera_.setPosition(tempPosition);
        }

        // Quick position buttons
        if (ImGui::Button("Reset Position")) {
            camera_.setPosition(glm::vec3(0.0f, 0.0f, -2.5f));
        }
        ImGui::SameLine();
        if (ImGui::Button("View Origin")) {
            camera_.setPosition(glm::vec3(0.0f, 0.0f, 5.0f));
        }
    }

    // Camera Rotation Controls
    if (ImGui::CollapsingHeader("Rotation Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
        glm::vec3 tempRotation = camera_.rotation;
        if (ImGui::SliderFloat3("Rotation (degrees)", &tempRotation.x, -180.0f, 180.0f, "%.1f°")) {
            camera_.setRotation(tempRotation);
        }

        // Quick rotation buttons
        if (ImGui::Button("Reset Rotation")) {
            camera_.setRotation(glm::vec3(0.0f));
        }
        ImGui::SameLine();
        if (ImGui::Button("Look Down")) {
            camera_.setRotation(glm::vec3(-45.0f, 0.0f, 0.0f));
        }
    }

    // Camera Settings
    if (ImGui::CollapsingHeader("Camera Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Movement and rotation speed controls
        float movementSpeed = camera_.movementSpeed;
        if (ImGui::SliderFloat("Movement Speed", &movementSpeed, 0.1f, 50.0f, "%.1f")) {
            camera_.setMovementSpeed(movementSpeed);
        }

        float rotationSpeed = camera_.rotationSpeed;
        if (ImGui::SliderFloat("Rotation Speed", &rotationSpeed, 0.01f, 2.0f, "%.2f")) {
            camera_.setRotationSpeed(rotationSpeed);
        }

        // Field of view control
        float currentFov = camera_.fov;
        if (ImGui::SliderFloat("Field of View", &currentFov, 30.0f, 120.0f, "%.1f°")) {
            const float aspectRatio = float(windowSize_.width) / windowSize_.height;
            camera_.setPerspective(currentFov, aspectRatio, camera_.znear, camera_.zfar);
        }

        // Near and far plane controls
        float nearPlane = camera_.znear;
        float farPlane = camera_.zfar;
        if (ImGui::SliderFloat("Near Plane", &nearPlane, 0.001f, 10.0f, "%.3f")) {
            const float aspectRatio = float(windowSize_.width) / windowSize_.height;
            camera_.setPerspective(camera_.fov, aspectRatio, nearPlane, camera_.zfar);
        }
        if (ImGui::SliderFloat("Far Plane", &farPlane, 10.0f, 10000.0f, "%.0f")) {
            const float aspectRatio = float(windowSize_.width) / windowSize_.height;
            camera_.setPerspective(camera_.fov, aspectRatio, camera_.znear, farPlane);
        }
    }

    // Camera Presets
    if (ImGui::CollapsingHeader("Presets")) {
        if (ImGui::Button("Helmet View")) {
            camera_.setPosition(glm::vec3(0.0f, 0.0f, 2.0f));
            camera_.setRotation(glm::vec3(0.0f));
            camera_.type = hlab::Camera::CameraType::firstperson;
        }
        ImGui::SameLine();
        if (ImGui::Button("Side View")) {
            camera_.setPosition(glm::vec3(3.0f, 0.0f, 0.0f));
            camera_.setRotation(glm::vec3(0.0f, -90.0f, 0.0f));
        }

        if (ImGui::Button("Top View")) {
            camera_.setPosition(glm::vec3(0.0f, 5.0f, 0.0f));
            camera_.setRotation(glm::vec3(-90.0f, 0.0f, 0.0f));
        }
        ImGui::SameLine();
        if (ImGui::Button("Perspective View")) {
            camera_.setPosition(glm::vec3(2.0f, 2.0f, 2.0f));
            camera_.setRotation(glm::vec3(-25.0f, -45.0f, 0.0f));
        }
    }

    // Controls Information
    if (ImGui::CollapsingHeader("Controls Help")) {
        ImGui::Text("Keyboard Controls:");
        ImGui::BulletText("WASD: Move forward/back/left/right");
        ImGui::BulletText("Q/E: Move up/down");
        ImGui::BulletText("F2: Toggle camera mode");
        ImGui::BulletText("F3: Print camera info to console");
        ImGui::BulletText("F4: Toggle frustum culling");
    }

    ImGui::End();
}

void Application::handleMouseMove(int32_t x, int32_t y)
{
    if (ImGui::GetIO().WantCaptureMouse) {
        mouseState_.position = glm::vec2((float)x, (float)y);
        return;
    }

    int32_t dx = (int32_t)mouseState_.position.x - x;
    int32_t dy = (int32_t)mouseState_.position.y - y;

    if (mouseState_.buttons.left) {
        camera_.rotate(glm::vec3(-dy * camera_.rotationSpeed, -dx * camera_.rotationSpeed, 0.0f));
    }

    if (mouseState_.buttons.right) {
        glm::vec3 forward = glm::vec3(camera_.matrices.view[0][2], camera_.matrices.view[1][2], camera_.matrices.view[2][2]);
        camera_.translate(forward * (float)dy * 0.005f);
    }

    if (mouseState_.buttons.middle) {
        glm::vec3 right = glm::vec3(camera_.matrices.view[0][0], camera_.matrices.view[1][0], camera_.matrices.view[2][0]);
        glm::vec3 up = glm::vec3(camera_.matrices.view[0][1], camera_.matrices.view[1][1], camera_.matrices.view[2][1]);
        camera_.translate(right * (float)dx * -0.005f + up * (float)dy * 0.005f);
    }

    mouseState_.position = glm::vec2((float)x, (float)y);
}

void Application::updatePerformanceMetrics(float deltaTime)
{
    TRACY_CPU_SCOPE("Application::updatePerformanceMetrics");

    // Update CPU FPS
    framesSinceLastUpdate_++;
    fpsUpdateTimer_ += deltaTime;

    if (fpsUpdateTimer_ >= kFpsUpdateInterval) {
        TRACY_CPU_SCOPE("FPS Calculation");
        currentFPS_ = static_cast<float>(framesSinceLastUpdate_) / fpsUpdateTimer_;
        currentFPS_ = std::clamp(currentFPS_, 0.1f, 1000.0f);

        framesSinceLastUpdate_ = 0;
        fpsUpdateTimer_ = 0.0f;

        // Update Tracy plots
        if (tracyProfiler_ && tracyProfiler_->isTracySupported()) {
            tracyProfiler_->plot("FPS_Average", currentFPS_);
            tracyProfiler_->plot("Frame_Time_ms", 1000.0f / std::max(currentFPS_, 1.0f));
        }
    }

    // Update GPU timing - check less frequently to allow GPU queries to complete
    gpuFramesSinceLastUpdate_++;
    gpuTimeUpdateTimer_ += deltaTime;

    // Wait a bit longer for GPU results (0.2s instead of 0.1s)
    if (gpuTimeUpdateTimer_ >= (kGpuTimeUpdateInterval * 2.0f)) {
        TRACY_CPU_SCOPE("GPU Time Update");
        // Get GPU time for frames that should be ready
        if (gpuTimer_.isTimestampSupported()) {
            for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
                // Try to get results even if not marked as ready
                float newGpuTime = gpuTimer_.getGpuTimeMs(i);
                if (newGpuTime > 0.0f) {
                    currentGpuTimeMs_ = newGpuTime;

                    // Update Tracy plots
                    if (tracyProfiler_ && tracyProfiler_->isTracySupported()) {
                        tracyProfiler_->plot("GPU_Time_Average_ms", currentGpuTimeMs_);
                        tracyProfiler_->plot("GPU_FPS_Equivalent",
                                             1000.0f / std::max(currentGpuTimeMs_, 0.1f));
                    }
                    break;
                }
            }
        }

        gpuFramesSinceLastUpdate_ = 0;
        gpuTimeUpdateTimer_ = 0.0f;
    }

    // Track additional metrics in Tracy
    if (tracyProfiler_ && tracyProfiler_->isTracySupported()) {
        TRACY_CPU_SCOPE("Additional Tracy Metrics");
        // Track model count and vertex data
        size_t totalVertices = 0;
        size_t totalTriangles = 0;
        size_t visibleModels = 0;

        for (const auto& model : models_) {
            if (model->visible()) {
                visibleModels++;
                // Add vertex/triangle counting if your Model class supports it
                // totalVertices += model->getVertexCount();
                // totalTriangles += model->getTriangleCount();
            }
        }

        tracyProfiler_->plot("Visible_Models", static_cast<float>(visibleModels));
        tracyProfiler_->plot("Total_Models", static_cast<float>(models_.size()));

        // Track memory usage if available
        // tracyProfiler_.plot("GPU_Memory_MB", getGPUMemoryUsageMB());

        // Track rendering settings
        if (renderer_) {
            tracyProfiler_->plot("Shadows_Enabled", renderer_->optionsUBO().shadowOn ? 1.0f : 0.0f);
            tracyProfiler_->plot("Textures_Enabled",
                                 renderer_->optionsUBO().textureOn ? 1.0f : 0.0f);
            tracyProfiler_->plot("Frustum_Culling",
                                 renderer_->isFrustumCullingEnabled() ? 1.0f : 0.0f);
        }
    }
}

// ADD: SSAO Control window method (new function)
void Application::renderSSAOControlWindow()
{
    ImGui::SetNextWindowPos(ImVec2(10, 780), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("SSAO Controls")) {
        ImGui::End();
        return;
    }

    if (ImGui::CollapsingHeader("SSAO Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("SSAO Radius", &renderer_->ssaoOptionsUBO().ssaoRadius, 0.01f, 1.0f,
                           "%.3f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Controls the sample radius for SSAO\n"
                              "Smaller values = fine detail occlusion\n"
                              "Larger values = broader occlusion");
        }

        ImGui::SliderFloat("SSAO Bias", &renderer_->ssaoOptionsUBO().ssaoBias, 0.0f, 0.1f, "%.4f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Bias to prevent self-occlusion artifacts\n"
                              "Too low = acne/noise\n"
                              "Too high = loss of detail");
        }

        int sampleCount = static_cast<int>(renderer_->ssaoOptionsUBO().ssaoSampleCount);
        if (ImGui::SliderInt("Sample Count", &sampleCount, 4, 64)) {
            renderer_->ssaoOptionsUBO().ssaoSampleCount = static_cast<int>(sampleCount);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Number of samples per pixel\n"
                              "More samples = better quality, lower performance");
        }

        ImGui::SliderFloat("SSAO Power", &renderer_->ssaoOptionsUBO().ssaoPower, 0.5f, 4.0f,
                           "%.2f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Controls the contrast of the SSAO effect\n"
                              "Higher values = stronger contrast");
        }
    }

    if (ImGui::CollapsingHeader("Presets")) {
        if (ImGui::Button("Subtle")) {
            renderer_->ssaoOptionsUBO().ssaoRadius = 0.05f;
            renderer_->ssaoOptionsUBO().ssaoBias = 0.025f;
            renderer_->ssaoOptionsUBO().ssaoSampleCount = 16;
            renderer_->ssaoOptionsUBO().ssaoPower = 1.5f;
        }
        ImGui::SameLine();
        if (ImGui::Button("Default")) {
            renderer_->ssaoOptionsUBO().ssaoRadius = 0.1f;
            renderer_->ssaoOptionsUBO().ssaoBias = 0.025f;
            renderer_->ssaoOptionsUBO().ssaoSampleCount = 16;
            renderer_->ssaoOptionsUBO().ssaoPower = 2.0f;
        }

        if (ImGui::Button("Strong")) {
            renderer_->ssaoOptionsUBO().ssaoRadius = 0.2f;
            renderer_->ssaoOptionsUBO().ssaoBias = 0.02f;
            renderer_->ssaoOptionsUBO().ssaoSampleCount = 32;
            renderer_->ssaoOptionsUBO().ssaoPower = 3.0f;
        }
        ImGui::SameLine();
        if (ImGui::Button("High Quality")) {
            renderer_->ssaoOptionsUBO().ssaoRadius = 0.15f;
            renderer_->ssaoOptionsUBO().ssaoBias = 0.015f;
            renderer_->ssaoOptionsUBO().ssaoSampleCount = 64;
            renderer_->ssaoOptionsUBO().ssaoPower = 2.5f;
        }
    }

    ImGui::End();
}

} // namespace hlab