#include "Ex11_PostProcessingExample.h"
#include "engine/Logger.h"

#include <chrono>
#include <thread>
#include <filesystem>
#include <stdexcept>

using namespace hlab;

Ex11_PostProcessingExample::Ex11_PostProcessingExample()
    : window_{}, ctx_{window_.getRequiredExtensions(), true},
      windowSize_{window_.getFramebufferSize()},
      swapchain_{ctx_, window_.createSurface(ctx_.instance()), windowSize_, true},
      shaderManager_{ctx_,
                     kShaderPathPrefix,
                     {{"gui", {"imgui.vert", "imgui.frag"}},
                      {"sky", {"skybox.vert.spv", "skybox.frag.spv"}},
                      {"post", {"post.vert", "post.frag"}}}},
      guiRenderer_{ctx_, shaderManager_, swapchain_.colorFormat()}, skyTextures_{ctx_},
      skyPipeline_(ctx_, shaderManager_), postPipeline_(ctx_, shaderManager_),
      hdrColorBuffer_(ctx_), samplerLinearRepeat_(ctx_), samplerLinearClamp_(ctx_)
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

    samplerLinearRepeat_.createLinearRepeat();
    samplerLinearClamp_.createLinearClamp();

    // Camera setup
    const float aspectRatio = float(windowSize_.width) / windowSize_.height;
    camera_.type = hlab::Camera::CameraType::firstperson;
    camera_.setPosition(glm::vec3(0.0f, 0.0f, -2.5f));
    camera_.setRotation(glm::vec3(0.0f));
    camera_.updateViewMatrix();
    camera_.setPerspective(75.0f, aspectRatio, 0.1f, 256.0f);

    // Initialize skybox and post-processing
    initializeSkybox();
    initializePostProcessing();
}

Ex11_PostProcessingExample::~Ex11_PostProcessingExample()
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

void Ex11_PostProcessingExample::initializeSkybox()
{
    // Create HDR color buffer for skybox rendering
    hdrColorBuffer_.createImage(VK_FORMAT_R16G16B16A16_SFLOAT, windowSize_.width,
                                windowSize_.height, VK_SAMPLE_COUNT_1_BIT,
                                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, 0, VK_IMAGE_VIEW_TYPE_2D);

    // Create skybox pipeline (now renders to HDR buffer instead of swapchain)
    skyPipeline_.createByName("sky", VK_FORMAT_R16G16B16A16_SFLOAT, ctx_.depthFormat(),
                              VK_SAMPLE_COUNT_1_BIT);

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

    // Create HDR sky options uniform buffers
    skyOptionsUniforms_.clear();
    skyOptionsUniforms_.reserve(kMaxFramesInFlight);
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        skyOptionsUniforms_.emplace_back(ctx_, skyOptionsUBO_);
    }

    // Create descriptor sets for scene data and options (set 0)
    sceneDescriptorSets_.resize(kMaxFramesInFlight);
    for (size_t i = 0; i < kMaxFramesInFlight; i++) {
        sceneDescriptorSets_[i].create(ctx_,
                                       {
                                           sceneDataUniforms_[i].resourceBinding(), // binding 0
                                           skyOptionsUniforms_[i].resourceBinding() // binding 1
                                       });
    }

    // Create descriptor set for skybox textures (set 1)
    skyDescriptorSet_.create(ctx_, {skyTextures_.prefiltered().resourceBinding(),
                                    skyTextures_.irradiance().resourceBinding(),
                                    skyTextures_.brdfLUT().resourceBinding()});
}

void Ex11_PostProcessingExample::initializePostProcessing()
{
    // Create post-processing pipeline
    postPipeline_.createByName("post", swapchain_.colorFormat(), ctx_.depthFormat(),
                               VK_SAMPLE_COUNT_1_BIT);

    // Set up HDR color buffer sampler
    hdrColorBuffer_.setSampler(samplerLinearClamp_.handle());

    // Create post-processing uniform buffers for each frame
    postProcessingOptionsUniforms_.clear();
    postProcessingOptionsUniforms_.reserve(kMaxFramesInFlight);
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        postProcessingOptionsUniforms_.emplace_back(ctx_, postProcessingOptionsUBO_);
    }

    // Create descriptor sets for post-processing (set 0: HDR texture + options uniform)
    postProcessingDescriptorSets_.resize(kMaxFramesInFlight);
    for (size_t i = 0; i < kMaxFramesInFlight; i++) {
        postProcessingDescriptorSets_[i].create(
            ctx_,
            {
                hdrColorBuffer_.resourceBinding(), // binding 0: sampler2D hdrColorBuffer
                postProcessingOptionsUniforms_[i]
                    .resourceBinding() // binding 1: PostProcessingOptions uniform
            });
    }
}

void Ex11_PostProcessingExample::mainLoop()
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

        updateGui(windowSize_);
        guiRenderer_.update();

        renderFrame();
    }
}

void Ex11_PostProcessingExample::renderFrame()
{
    check(vkWaitForFences(ctx_.device(), 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX));
    check(vkResetFences(ctx_.device(), 1, &inFlightFences_[currentFrame_]));

    // Update uniform buffers
    sceneDataUniforms_[currentFrame_].updateData();
    skyOptionsUniforms_[currentFrame_].updateData();
    postProcessingOptionsUniforms_[currentFrame_].updateData(); // Update post-processing options

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

void Ex11_PostProcessingExample::updateGui(VkExtent2D windowSize)
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

    // Camera info window
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
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

    // HDR Control window
    renderHDRControlWindow();

    // Post-processing Control window
    renderPostProcessingControlWindow();

    ImGui::Render();
}

void Ex11_PostProcessingExample::renderHDRControlWindow()
{
    ImGui::SetNextWindowPos(ImVec2(320, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 350), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("HDR Skybox Controls")) {
        ImGui::End();
        return;
    }

    // HDR Environment Controls
    if (ImGui::CollapsingHeader("HDR Environment", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Environment Intensity", &skyOptionsUBO_.environmentIntensity, 0.0f,
                           10.0f, "%.2f");
    }

    // Environment Map Controls
    if (ImGui::CollapsingHeader("Environment Map", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Roughness Level", &skyOptionsUBO_.roughnessLevel, 0.0f, 8.0f, "%.1f");

        bool useIrradiance = skyOptionsUBO_.useIrradianceMap != 0;
        if (ImGui::Checkbox("Use Irradiance Map", &useIrradiance)) {
            skyOptionsUBO_.useIrradianceMap = useIrradiance ? 1 : 0;
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
        bool showMipLevels = skyOptionsUBO_.showMipLevels != 0;
        if (ImGui::Checkbox("Show Mip Levels", &showMipLevels)) {
            skyOptionsUBO_.showMipLevels = showMipLevels ? 1 : 0;
        }

        bool showCubeFaces = skyOptionsUBO_.showCubeFaces != 0;
        if (ImGui::Checkbox("Show Cube Faces", &showCubeFaces)) {
            skyOptionsUBO_.showCubeFaces = showCubeFaces ? 1 : 0;
        }
    }

    // Simplified Presets
    if (ImGui::CollapsingHeader("Presets")) {
        if (ImGui::Button("Default")) {
            skyOptionsUBO_.environmentIntensity = 1.0f;
            skyOptionsUBO_.roughnessLevel = 0.5f;
            skyOptionsUBO_.useIrradianceMap = 0;
            skyOptionsUBO_.showMipLevels = 0;
            skyOptionsUBO_.showCubeFaces = 0;
        }
        ImGui::SameLine();
        if (ImGui::Button("High Exposure")) {
            skyOptionsUBO_.environmentIntensity = 1.5f;
        }
        ImGui::SameLine();
        if (ImGui::Button("Low Exposure")) {
            skyOptionsUBO_.environmentIntensity = 0.8f;
        }

        if (ImGui::Button("Sharp Reflections")) {
            skyOptionsUBO_.roughnessLevel = 0.0f;
            skyOptionsUBO_.useIrradianceMap = 0;
        }
        ImGui::SameLine();
        if (ImGui::Button("Diffuse Lighting")) {
            skyOptionsUBO_.useIrradianceMap = 1;
        }
    }

    ImGui::End();
}

void Ex11_PostProcessingExample::renderPostProcessingControlWindow()
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
        ImGui::Combo("Tone Mapping Type", &postProcessingOptionsUBO_.toneMappingType,
                     toneMappingNames, IM_ARRAYSIZE(toneMappingNames));

        ImGui::SliderFloat("Exposure", &postProcessingOptionsUBO_.exposure, 0.1f, 5.0f, "%.2f");
        ImGui::SliderFloat("Gamma", &postProcessingOptionsUBO_.gamma, 1.0f / 2.2f, 2.2f, "%.2f");

        if (postProcessingOptionsUBO_.toneMappingType == 7) { // Reinhard Extended
            ImGui::SliderFloat("Max White", &postProcessingOptionsUBO_.maxWhite, 1.0f, 20.0f,
                               "%.1f");
        }
    }

    // Color Grading Controls
    if (ImGui::CollapsingHeader("Color Grading", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Contrast", &postProcessingOptionsUBO_.contrast, 0.0f, 3.0f, "%.2f");
        ImGui::SliderFloat("Brightness", &postProcessingOptionsUBO_.brightness, -1.0f, 1.0f,
                           "%.2f");
        ImGui::SliderFloat("Saturation", &postProcessingOptionsUBO_.saturation, 0.0f, 2.0f, "%.2f");
        ImGui::SliderFloat("Vibrance", &postProcessingOptionsUBO_.vibrance, -1.0f, 1.0f, "%.2f");
    }

    // Effects Controls
    if (ImGui::CollapsingHeader("Effects")) {
        ImGui::SliderFloat("Vignette Strength", &postProcessingOptionsUBO_.vignetteStrength, 0.0f,
                           1.0f, "%.2f");
        if (postProcessingOptionsUBO_.vignetteStrength > 0.0f) {
            ImGui::SliderFloat("Vignette Radius", &postProcessingOptionsUBO_.vignetteRadius, 0.1f,
                               1.5f, "%.2f");
        }

        ImGui::SliderFloat("Film Grain", &postProcessingOptionsUBO_.filmGrainStrength, 0.0f, 0.2f,
                           "%.3f");
        ImGui::SliderFloat("Chromatic Aberration", &postProcessingOptionsUBO_.chromaticAberration,
                           0.0f, 5.0f, "%.1f");
    }

    // Debug Controls
    if (ImGui::CollapsingHeader("Debug Visualization")) {
        const char* debugModeNames[] = {"Off", "Tone Mapping Comparison", "Color Channels",
                                        "Split Comparison"};
        ImGui::Combo("Debug Mode", &postProcessingOptionsUBO_.debugMode, debugModeNames,
                     IM_ARRAYSIZE(debugModeNames));

        if (postProcessingOptionsUBO_.debugMode == 2) { // Color Channels
            const char* channelNames[] = {"All",       "Red Only", "Green Only",
                                          "Blue Only", "Alpha",    "Luminance"};
            ImGui::Combo("Show Channel", &postProcessingOptionsUBO_.showOnlyChannel, channelNames,
                         IM_ARRAYSIZE(channelNames));
        }

        if (postProcessingOptionsUBO_.debugMode == 3) { // Split Comparison
            ImGui::SliderFloat("Split Position", &postProcessingOptionsUBO_.debugSplit, 0.0f, 1.0f,
                               "%.2f");
        }
    }

    // Presets
    if (ImGui::CollapsingHeader("Presets")) {
        if (ImGui::Button("Default")) {
            postProcessingOptionsUBO_.toneMappingType = 2; // ACES
            postProcessingOptionsUBO_.exposure = 1.0f;
            postProcessingOptionsUBO_.gamma = 2.2f;
            postProcessingOptionsUBO_.contrast = 1.0f;
            postProcessingOptionsUBO_.brightness = 0.0f;
            postProcessingOptionsUBO_.saturation = 1.0f;
            postProcessingOptionsUBO_.vibrance = 0.0f;
            postProcessingOptionsUBO_.vignetteStrength = 0.0f;
            postProcessingOptionsUBO_.filmGrainStrength = 0.0f;
            postProcessingOptionsUBO_.chromaticAberration = 0.0f;
            postProcessingOptionsUBO_.debugMode = 0;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cinematic")) {
            postProcessingOptionsUBO_.toneMappingType = 3; // Uncharted 2
            postProcessingOptionsUBO_.exposure = 1.2f;
            postProcessingOptionsUBO_.contrast = 1.1f;
            postProcessingOptionsUBO_.saturation = 0.9f;
            postProcessingOptionsUBO_.vignetteStrength = 0.3f;
            postProcessingOptionsUBO_.vignetteRadius = 0.8f;
            postProcessingOptionsUBO_.filmGrainStrength = 0.02f;
        }

        if (ImGui::Button("High Contrast")) {
            postProcessingOptionsUBO_.contrast = 1.5f;
            postProcessingOptionsUBO_.brightness = 0.1f;
            postProcessingOptionsUBO_.saturation = 1.3f;
            postProcessingOptionsUBO_.vignetteStrength = 0.2f;
        }
        ImGui::SameLine();
        if (ImGui::Button("Low Contrast")) {
            postProcessingOptionsUBO_.contrast = 0.7f;
            postProcessingOptionsUBO_.brightness = 0.05f;
            postProcessingOptionsUBO_.saturation = 0.8f;
        }

        if (ImGui::Button("Show Tone Mapping")) {
            postProcessingOptionsUBO_.debugMode = 1;
            postProcessingOptionsUBO_.exposure = 2.0f;
        }
    }

    ImGui::End();
}

void Ex11_PostProcessingExample::recordCommandBuffer(CommandBuffer& cmd, uint32_t imageIndex,
                                                     VkExtent2D windowSize)
{
    vkResetCommandBuffer(cmd.handle(), 0);
    VkCommandBufferBeginInfo cmdBufferBeginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    check(vkBeginCommandBuffer(cmd.handle(), &cmdBufferBeginInfo));

    // === PASS 1: Render skybox to HDR color buffer ===

    // Transition HDR color buffer to color attachment
    hdrColorBuffer_.transitionTo(cmd.handle(), VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                 VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

    // HDR skybox rendering pass
    VkClearColorValue hdrClearColorValue = {{0.0f, 0.0f, 0.0f, 1.0f}}; // Black clear for HDR

    VkRenderingAttachmentInfo hdrColorAttachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    hdrColorAttachment.imageView = hdrColorBuffer_.view();
    hdrColorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    hdrColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    hdrColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    hdrColorAttachment.clearValue.color = hdrClearColorValue;

    VkRenderingInfo hdrRenderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO};
    hdrRenderingInfo.renderArea = {0, 0, windowSize.width, windowSize.height};
    hdrRenderingInfo.layerCount = 1;
    hdrRenderingInfo.colorAttachmentCount = 1;
    hdrRenderingInfo.pColorAttachments = &hdrColorAttachment;

    vkCmdBeginRendering(cmd.handle(), &hdrRenderingInfo);

    // Set viewport and scissor
    VkViewport viewport{0.0f, 0.0f, (float)windowSize.width, (float)windowSize.height, 0.0f, 1.0f};
    VkRect2D scissor{0, 0, windowSize.width, windowSize.height};
    vkCmdSetViewport(cmd.handle(), 0, 1, &viewport);
    vkCmdSetScissor(cmd.handle(), 0, 1, &scissor);

    // Render skybox to HDR buffer
    vkCmdBindPipeline(cmd.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, skyPipeline_.pipeline());

    const auto skyDescriptorSets =
        std::vector{sceneDescriptorSets_[currentFrame_].handle(), skyDescriptorSet_.handle()};

    vkCmdBindDescriptorSets(
        cmd.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, skyPipeline_.pipelineLayout(), 0,
        static_cast<uint32_t>(skyDescriptorSets.size()), skyDescriptorSets.data(), 0, nullptr);

    // Draw skybox - 36 vertices from hardcoded data in shader
    vkCmdDraw(cmd.handle(), 36, 1, 0, 0);

    vkCmdEndRendering(cmd.handle());

    // === PASS 2: Post-process HDR buffer to swapchain ===

    // Transition HDR color buffer to shader read
    hdrColorBuffer_.transitionTo(cmd.handle(), VK_ACCESS_2_SHADER_READ_BIT,
                                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                 VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);

    // Transition swapchain image to color attachment
    swapchain_.barrierHelper(imageIndex)
        .transitionTo(cmd.handle(), VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

    // Post-processing rendering pass
    VkClearColorValue postClearColorValue = {{0.0f, 0.0f, 0.0f, 1.0f}};

    VkRenderingAttachmentInfo postColorAttachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    postColorAttachment.imageView = swapchain_.imageView(imageIndex);
    postColorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    postColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    postColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    postColorAttachment.clearValue.color = postClearColorValue;

    VkRenderingInfo postRenderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO};
    postRenderingInfo.renderArea = {0, 0, windowSize.width, windowSize.height};
    postRenderingInfo.layerCount = 1;
    postRenderingInfo.colorAttachmentCount = 1;
    postRenderingInfo.pColorAttachments = &postColorAttachment;

    vkCmdBeginRendering(cmd.handle(), &postRenderingInfo);

    vkCmdSetViewport(cmd.handle(), 0, 1, &viewport);
    vkCmdSetScissor(cmd.handle(), 0, 1, &scissor);

    // Render post-processing fullscreen quad
    vkCmdBindPipeline(cmd.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, postPipeline_.pipeline());

    const auto postDescriptorSets =
        std::vector{postProcessingDescriptorSets_[currentFrame_].handle()};

    vkCmdBindDescriptorSets(
        cmd.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, postPipeline_.pipelineLayout(), 0,
        static_cast<uint32_t>(postDescriptorSets.size()), postDescriptorSets.data(), 0, nullptr);

    // Draw fullscreen quad - 6 vertices from hardcoded data in post.vert
    vkCmdDraw(cmd.handle(), 6, 1, 0, 0);

    vkCmdEndRendering(cmd.handle());

    // === PASS 3: Draw GUI on top ===

    // Draw GUI on top of the post-processed image
    guiRenderer_.draw(cmd.handle(), swapchain_.imageView(imageIndex), viewport);

    // Transition swapchain image to present
    swapchain_.barrierHelper(imageIndex)
        .transitionTo(cmd.handle(), VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                      VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);

    check(vkEndCommandBuffer(cmd.handle()));
}

void Ex11_PostProcessingExample::submitFrame(CommandBuffer& commandBuffer,
                                             VkSemaphore waitSemaphore, VkSemaphore signalSemaphore,
                                             VkFence fence)
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

void Ex11_PostProcessingExample::handleMouseMove(int32_t x, int32_t y)
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
void Ex11_PostProcessingExample::keyCallback(GLFWwindow* window, int key, int scancode, int action,
                                             int mods)
{
    Ex11_PostProcessingExample* example =
        static_cast<Ex11_PostProcessingExample*>(glfwGetWindowUserPointer(window));
    if (example) {
        example->handleKeyInput(key, scancode, action, mods);
    }
}

void Ex11_PostProcessingExample::mouseButtonCallback(GLFWwindow* window, int button, int action,
                                                     int mods)
{
    Ex11_PostProcessingExample* example =
        static_cast<Ex11_PostProcessingExample*>(glfwGetWindowUserPointer(window));
    if (example) {
        example->handleMouseButton(button, action, mods);
    }
}

void Ex11_PostProcessingExample::cursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{
    Ex11_PostProcessingExample* example =
        static_cast<Ex11_PostProcessingExample*>(glfwGetWindowUserPointer(window));
    if (example) {
        example->handleCursorPos(xpos, ypos);
    }
}

void Ex11_PostProcessingExample::scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    Ex11_PostProcessingExample* example =
        static_cast<Ex11_PostProcessingExample*>(glfwGetWindowUserPointer(window));
    if (example) {
        example->handleScroll(xoffset, yoffset);
    }
}

// Instance callback handlers
void Ex11_PostProcessingExample::handleKeyInput(int key, int scancode, int action, int mods)
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

void Ex11_PostProcessingExample::handleMouseButton(int button, int action, int mods)
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

void Ex11_PostProcessingExample::handleCursorPos(double xpos, double ypos)
{
    handleMouseMove(static_cast<int32_t>(xpos), static_cast<int32_t>(ypos));
}

void Ex11_PostProcessingExample::handleScroll(double xoffset, double yoffset)
{
    camera_.translate(glm::vec3(0.0f, 0.0f, (float)yoffset * 0.05f));
}