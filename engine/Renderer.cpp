#include "Renderer.h"
#include "Logger.h"
#include "TracyProfiler.h" // Add Tracy macros wrapper
#include <stb_image.h>

namespace hlab {

Renderer::Renderer(Context& ctx, ShaderManager& shaderManager, const uint32_t& kMaxFramesInFlight,
                   const string& kAssetsPathPrefix, const string& kShaderPathPrefix_,
                   vector<unique_ptr<Model>>& models, VkFormat outColorFormat, VkFormat depthFormat,
                   uint32_t swapChainWidth, uint32_t swapChainHeight)
    : ctx_(ctx), shaderManager_(shaderManager), kMaxFramesInFlight_(kMaxFramesInFlight),
      kAssetsPathPrefix_(kAssetsPathPrefix), kShaderPathPrefix_(kShaderPathPrefix_),
      samplerShadow_(ctx), samplerLinearRepeat_(ctx), samplerLinearClamp_(ctx),
      samplerAnisoRepeat_(ctx), samplerAnisoClamp_(ctx),
      materialTextures_(make_unique<TextureManager>(ctx))
{
    createPipelines(outColorFormat, depthFormat);
    createTextures(swapChainWidth, swapChainHeight);
    createUniformBuffers();

    {
        vector<MaterialUBO> allMaterials;

        for (auto& m : models) {
            m->prepareForBindlessRendering(samplerLinearRepeat_, allMaterials, *materialTextures_);
        }

        materialBuffer_ = make_unique<StorageBuffer>(ctx_, allMaterials.data(),
                                                     sizeof(MaterialUBO) * allMaterials.size());
    }

    {
        // ... existing descriptor set creation code ...
        unordered_map<string, vector<string>> descriptorSetNames; // TODO: move to script
        descriptorSetNames["shadowMap"] = {"sceneOptions"};
        descriptorSetNames["pbrDeferred"] = {"sceneOptions", "material"};
        descriptorSetNames["sky"] = {"skyOptions", "sky"};
        descriptorSetNames["deferredLighting"] = {"deferredLightingData"};
        descriptorSetNames["post"] = {"postProcessing"};

        unordered_map<string, vector<vector<BindingInfo>>> bindingInfos =
            shaderManager_.bindingInfos();

        for (auto i : descriptorSetNames) {
            auto pipelineName = i.first;

            auto& bindings = bindingInfos.at(pipelineName);

            assert(bindings.size() == descriptorSetNames[pipelineName].size());

            for (int s = 0; s < bindings.size(); s++) {

                string setName = descriptorSetNames[pipelineName][s];

                if (perFrameDescriptorSets_.find(setName) != perFrameDescriptorSets_.end())
                    continue;
                if (descriptorSets_.find(setName) != descriptorSets_.end())
                    continue;

                vector<string> bindingNames;
                for (int b = 0; b < bindings[s].size(); b++) {
                    bindingNames.push_back(bindings[s][b].resourceName);
                }

                bool perFramesSet = this->perFrameResources(bindingNames);

                if (perFramesSet) {
                    perFrameDescriptorSets_[setName].resize(kMaxFramesInFlight_);
                    for (uint32_t i = 0; i < kMaxFramesInFlight_; i++) {
                        // Collect resources for this descriptor set
                        vector<reference_wrapper<Resource>> resources;

                        for (const string& resourceName : bindingNames) {
                            addResource(resourceName, i, resources);
                        }

                        // Create the descriptor set with collected resources
                        perFrameDescriptorSets_[setName][i].create(
                            ctx_, pipelines_[pipelineName]->layouts()[s], resources);
                    }
                } else {
                    // Collect resources for non-per-frame descriptor set
                    vector<reference_wrapper<Resource>> resources;

                    for (const string& resourceName : bindingNames) {
                        addResource(resourceName, uint32_t(-1), resources);
                    }

                    // Create the descriptor set with collected resources
                    descriptorSets_[setName].create(ctx_, pipelines_[pipelineName]->layouts()[s],
                                                    resources);
                }
            }

            // Update pipeline's descriptor sets with the created descriptor sets for this pipeline
            vector<vector<reference_wrapper<DescriptorSet>>> pipelineDescriptorSets;
            pipelineDescriptorSets.resize(kMaxFramesInFlight_);

            for (uint32_t frameIndex = 0; frameIndex < kMaxFramesInFlight_; ++frameIndex) {
                pipelineDescriptorSets[frameIndex].reserve(descriptorSetNames[pipelineName].size());

                for (size_t setIndex = 0; setIndex < descriptorSetNames[pipelineName].size();
                     ++setIndex) {
                    const string& setName = descriptorSetNames[pipelineName][setIndex];

                    // Check if this is a per-frame descriptor set
                    if (perFrameDescriptorSets_.find(setName) != perFrameDescriptorSets_.end()) {
                        // For per-frame sets, use the specific frame
                        pipelineDescriptorSets[frameIndex].emplace_back(
                            std::ref(perFrameDescriptorSets_[setName][frameIndex]));
                    } else if (descriptorSets_.find(setName) != descriptorSets_.end()) {
                        // For non-per-frame sets, use the same descriptor set for all frames
                        pipelineDescriptorSets[frameIndex].emplace_back(
                            std::ref(descriptorSets_[setName]));
                    }
                }
            }

            // Set the descriptor sets on the pipeline
            pipelines_[pipelineName]->setDescriptorSets(pipelineDescriptorSets);
        }
    }
}

void Renderer::createUniformBuffers()
{
    const VkDevice device = ctx_.device();

    // Initialize uniform buffer map with proper keys
    const vector<string> bufferNames = {"sceneData", "skyOptions",  "options",
                                        "boneData",  "postOptions", "ssaoOptions"};

    for (const auto& name : bufferNames) {
        perFrameUniformBuffers_[name].clear();
        perFrameUniformBuffers_[name].reserve(kMaxFramesInFlight_);
    }

    // Create uniform buffers for each type and frame
    for (uint32_t i = 0; i < kMaxFramesInFlight_; ++i) {
        // Scene uniform buffers
        auto sceneBuffer = make_unique<MappedBuffer>(ctx_);
        sceneBuffer->createUniformBuffer(sceneUBO_);
        perFrameUniformBuffers_["sceneData"].emplace_back(std::move(sceneBuffer));

        // Options uniform buffers
        auto optionsBuffer = make_unique<MappedBuffer>(ctx_);
        optionsBuffer->createUniformBuffer(optionsUBO_);
        perFrameUniformBuffers_["options"].emplace_back(std::move(optionsBuffer));

        // Sky options uniform buffers
        auto skyOptionsBuffer = make_unique<MappedBuffer>(ctx_);
        skyOptionsBuffer->createUniformBuffer(skyOptionsUBO_);
        perFrameUniformBuffers_["skyOptions"].emplace_back(std::move(skyOptionsBuffer));

        // Post options uniform buffers
        auto postOptionsBuffer = make_unique<MappedBuffer>(ctx_);
        postOptionsBuffer->createUniformBuffer(postOptionsUBO_);
        perFrameUniformBuffers_["postOptions"].emplace_back(std::move(postOptionsBuffer));

        // SSAO options uniform buffers
        auto ssaoOptionsBuffer = make_unique<MappedBuffer>(ctx_);
        ssaoOptionsBuffer->createUniformBuffer(ssaoOptionsUBO_);
        perFrameUniformBuffers_["ssaoOptions"].emplace_back(std::move(ssaoOptionsBuffer));

        // Bone data uniform buffers
        auto boneDataBuffer = make_unique<MappedBuffer>(ctx_);
        boneDataBuffer->createUniformBuffer(boneDataUBO_);
        perFrameUniformBuffers_["boneData"].emplace_back(std::move(boneDataBuffer));
    }
}

void Renderer::update(Camera& camera, vector<unique_ptr<Model>>& models, uint32_t currentFrame,
                      double time)
{
    {
        // Update view frustum based on current camera view-projection matrix
        updateViewFrustum(camera.matrices.perspective * camera.matrices.view);
    }

    {
        updateWorldBounds(models);
    }

    {
        updateBoneData(models, currentFrame);
    }

    {
        performFrustumCulling(models);
    }

    {
        // Update all uniform buffers using direct iteration over the map
        for (const auto& [bufferName, bufferVector] : perFrameUniformBuffers_) {
            bufferVector[currentFrame]->updateFromCpuData();
        }
    }
}

void Renderer::updateBoneData(const vector<unique_ptr<Model>>& models, uint32_t currentFrame)
{
    // Reset bone data
    boneDataUBO_.animationData.x = 0.0f;
    for (int i = 0; i < 65; ++i) {
        boneDataUBO_.boneMatrices[i] = glm::mat4(1.0f);
    }

    // Check if any model has animation data
    bool hasAnyAnimation = false;
    for (const auto& model : models) {
        if (model->hasAnimations() && model->hasBones()) {
            hasAnyAnimation = true;

            // Get bone matrices from the first animated model
            const auto& boneMatrices = model->getBoneMatrices();

            // Copy bone matrices (up to 65 bones)
            const size_t maxBones = 65;
            size_t bonesToCopy = (boneMatrices.size() < maxBones) ? boneMatrices.size() : maxBones;
            for (size_t i = 0; i < bonesToCopy; ++i) {
                boneDataUBO_.boneMatrices[i] = boneMatrices[i];
            }

            break; // For now, use the first animated model
        }
    }

    boneDataUBO_.animationData.x = float(hasAnyAnimation);

    // DEBUG: Log hasAnimation state
    static bool lastHasAnimation = false;
    if (lastHasAnimation != hasAnyAnimation) {
        printLog("hasAnimation changed to: {}", hasAnyAnimation);
        lastHasAnimation = hasAnyAnimation;
    }

    // Update the GPU buffer using the consolidated map structure
    perFrameUniformBuffers_["boneData"][currentFrame]->updateFromCpuData();
}

void Renderer::draw(VkCommandBuffer cmd, uint32_t currentFrame, VkImageView swapchainImageView,
                    vector<unique_ptr<Model>>& models, VkViewport viewport, VkRect2D scissor)
{
    TRACY_CPU_SCOPE("Renderer::draw");

    for (auto& renderNode : renderGraph_.renderNodes_) {

        if (renderNode.pipelineNames[0] == "deferredLighting") {
            TRACY_CPU_SCOPE("deferredLighting");
            pipelines_.at("deferredLighting")->dispatch(cmd, currentFrame); // Compute
            continue;
        }

        string mainTarget;
        vector<VkRenderingAttachmentInfo> colorAttachments{};
        VkRenderingAttachmentInfo depthAttachment{};
        VkRenderingAttachmentInfo stencilAttachment{};
        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;

        // Direct rendering (no MSAA) - simplified path

        // Handle color attachments
        for (const auto& colorTarget : renderNode.colorAttachments) {
            if (colorTarget == "swapchain") {
                colorAttachments.push_back(createColorAttachment(
                    swapchainImageView, VK_ATTACHMENT_LOAD_OP_CLEAR, {0.0f, 0.0f, 1.0f, 0.0f}));
            } else {
                if (mainTarget.empty()) {
                    mainTarget = colorTarget;
                }

                if (renderNode.pipelineNames[0] == "sky") {
                    colorAttachments.push_back(createColorAttachment(
                        imageBuffers_[colorTarget]->view(), VK_ATTACHMENT_LOAD_OP_LOAD,
                        {0.0f, 0.0f, 0.5f, 0.0f}));
                } else {
                    colorAttachments.push_back(createColorAttachment(
                        imageBuffers_[colorTarget]->view(), VK_ATTACHMENT_LOAD_OP_CLEAR,
                        {0.0f, 0.0f, 0.5f, 0.0f}));
                }
            }
        }

        // Handle depth attachment
        if (!renderNode.depthAttachment.empty()) {
            if (mainTarget.empty()) {
                mainTarget = renderNode.depthAttachment;
            }
            imageBuffers_[renderNode.depthAttachment]->transitionToDepthStencilAttachment(cmd);

            if (renderNode.pipelineNames[0] == "sky") {
                depthAttachment = createDepthAttachment(
                    imageBuffers_[renderNode.depthAttachment]->attachmentView(),
                    VK_ATTACHMENT_LOAD_OP_LOAD, 1.0f);
            } else {
                depthAttachment = createDepthAttachment(
                    imageBuffers_[renderNode.depthAttachment]->attachmentView(),
                    VK_ATTACHMENT_LOAD_OP_CLEAR, 1.0f);
            }

            renderingInfo.pDepthAttachment = &depthAttachment;
        }

        for (auto& pipelineName : renderNode.pipelineNames) {
            pipelines_.at(pipelineName)->submitBarriers(cmd, currentFrame);
        }

        uint32_t width = uint32_t(viewport.width);
        uint32_t height = uint32_t(viewport.height);
        if (!mainTarget.empty()) {
            width = imageBuffers_[mainTarget]->width();
            height = imageBuffers_[mainTarget]->height();
        }

        VkRect2D renderArea = {0, 0, width, height};
        renderingInfo.renderArea = renderArea;
        renderingInfo.layerCount = 1;
        if (colorAttachments.size() > 0) {
            renderingInfo.colorAttachmentCount = uint32_t(colorAttachments.size());
            renderingInfo.pColorAttachments = colorAttachments.data();
        }

        VkViewport viewport{0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f};
        VkRect2D scissor{0, 0, width, height};

        vkCmdBeginRendering(cmd, &renderingInfo);
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        // Process all pipelines for this render node
        {
            TRACY_CPU_SCOPE("ProcessPipelines");

            for (auto& pipelineName : renderNode.pipelineNames) {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  pipelines_.at(pipelineName)->pipeline());

                pipelines_.at(pipelineName)->bindDescriptorSets(cmd, currentFrame);

                if (pipelineName == "sky") {
                    TRACY_CPU_SCOPE("drawSky");
                    vkCmdDraw(cmd, 36, 1, 0, 0);
                    continue;
                }

                if (pipelineName == "post") {
                    TRACY_CPU_SCOPE("drawPost");
                    vkCmdDraw(cmd, 6, 1, 0, 0);
                    continue;
                }

                if (pipelineName == "shadowMap") {
                    TRACY_CPU_SCOPE("shadowMapSetup");
                    vkCmdSetDepthBias(cmd,
                                      1.1f,  // Constant factor
                                      0.0f,  // Clamp value
                                      2.0f); // Slope factor
                }

                // Render all visible models for this pipeline
                {
                    TRACY_CPU_SCOPE("DrawModels");

                    VkDeviceSize offsets[1]{0};
                    size_t visibleMeshCount = 0;
                    size_t totalMeshCount = 0;

                    for (size_t j = 0; j < models.size(); j++) {
                        if (!models[j]->visible()) {
                            continue;
                        }

                        // Render all meshes in this model
                        for (size_t i = 0; i < models[j]->meshes().size(); i++) {
                            auto& mesh = models[j]->meshes()[i];
                            totalMeshCount++;

                            // Skip culled meshes
                            if (mesh.isCulled) {
                                continue;
                            }
                            visibleMeshCount++;

                            PbrPushConstants pushConstants;
                            pushConstants.model = models[j]->modelMatrix();
                            pushConstants.materialIndex = mesh.materialIndex_;
                            memcpy(pushConstants.coeffs, models[j]->coeffs(),
                                   sizeof(pushConstants.coeffs));
                            vkCmdPushConstants(cmd, pipelines_.at(pipelineName)->pipelineLayout(),
                                               VK_SHADER_STAGE_VERTEX_BIT |
                                                   VK_SHADER_STAGE_FRAGMENT_BIT,
                                               0, sizeof(PbrPushConstants), &pushConstants);

                            // Bind vertex and index buffers
                            vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.vertexBuffer_, offsets);
                            vkCmdBindIndexBuffer(cmd, mesh.indexBuffer_, 0, VK_INDEX_TYPE_UINT32);

                            // Draw the mesh
                            vkCmdDrawIndexed(cmd, static_cast<uint32_t>(mesh.indices_.size()), 1, 0,
                                             0, 0);
                        }
                    }

                    // Track rendering statistics
                    TRACY_PLOT("VisibleMeshes", static_cast<int64_t>(visibleMeshCount));
                    TRACY_PLOT("TotalMeshes", static_cast<int64_t>(totalMeshCount));
                    TRACY_PLOT("CulledMeshes",
                               static_cast<int64_t>(totalMeshCount - visibleMeshCount));
                }
            }
        }

        vkCmdEndRendering(cmd);
    }
}

void Renderer::createPipelines(const VkFormat swapChainColorFormat, const VkFormat depthFormat)
{
    renderGraph_.readFromFile("RenderGraph.json");

    // Select optimal HDR format with proper priority (float formats first)
    VkFormat selectedHDRFormat =
        selectOptimalHDRFormat(false, false); // No alpha, moderate precision

    printLog("HDR Format Selection:");
    printLog("  Selected format: {} ({} bytes/pixel)", vkFormatToString(selectedHDRFormat),
             getFormatSize(selectedHDRFormat));

    // All pipelines use 1x samples (no MSAA) for educational simplicity
    pipelines_["pbrDeferred"] = make_unique<Pipeline>(
        ctx_, shaderManager_, PipelineConfig::createPbrDeferred(),
        vector<VkFormat>{VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R16G16B16A16_SFLOAT,
                         VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R8G8B8A8_UNORM},
        depthFormat, VK_SAMPLE_COUNT_1_BIT);

    pipelines_["sky"] = make_unique<Pipeline>(ctx_, shaderManager_, PipelineConfig::createSky(),
                                              vector<VkFormat>{selectedHDRFormat}, depthFormat,
                                              VK_SAMPLE_COUNT_1_BIT);

    pipelines_["post"] = make_unique<Pipeline>(ctx_, shaderManager_, PipelineConfig::createPost(),
                                               vector<VkFormat>{swapChainColorFormat}, depthFormat,
                                               VK_SAMPLE_COUNT_1_BIT);

    // Fix shadow map pipeline: depth-only pipelines should have no color attachments
    // Pass depth format as depthFormat parameter, not in outColorFormats
    pipelines_["shadowMap"] =
        make_unique<Pipeline>(ctx_, shaderManager_, PipelineConfig::createShadowMap(),
                              vector<VkFormat>{}, VK_FORMAT_D16_UNORM, VK_SAMPLE_COUNT_1_BIT);

    // Fix deferred lighting pipeline: compute pipelines don't need color/depth formats
    pipelines_["deferredLighting"] =
        make_unique<Pipeline>(ctx_, shaderManager_, PipelineConfig::createDeferredLighting(),
                              vector<VkFormat>{}, nullopt, VK_SAMPLE_COUNT_1_BIT);

    // Store the selected format for texture creation
    selectedHDRFormat_ = selectedHDRFormat;
}

void Renderer::createTextures(uint32_t swapchainWidth, uint32_t swapchainHeight)
{
    {
        TRACY_CPU_SCOPE("createSamplers");
        samplerLinearRepeat_.createLinearRepeat();
        samplerLinearClamp_.createLinearClamp();
        samplerAnisoRepeat_.createAnisoRepeat();
        samplerAnisoClamp_.createAnisoClamp();
        samplerShadow_.createShadow();
    }

    // Initialize image buffers (simplified - no MSAA)
    const vector<string> imageNames = {"depthStencil", "floatColor1",    "floatColor2",
                                       "shadowMap",    "prefilteredMap", "irradianceMap",
                                       "brdfLut",      "gAlbedo",        "gNormal",
                                       "gPosition",    "gMaterial"};

    for (const auto& name : imageNames) {
        imageBuffers_[name] = make_unique<Image2D>(ctx_);
    }

    {
        TRACY_CPU_SCOPE("loadIBLTextures");
        // Load IBL textures for PBR rendering
        string path = kAssetsPathPrefix_ + "textures/golden_gate_hills_4k/";

        printLog("Loading IBL textures...");
        printLog("  Prefiltered: {}", path + "specularGGX.ktx2");
        printLog("  Irradiance: {}", path + "diffuseLambertian.ktx2");
        printLog("  BRDF LUT: {}", path + "outputLUT.png");

        // Load prefiltered environment map (cubemap for specular reflections)
        imageBuffers_["prefilteredMap"]->createTextureFromKtx2(path + "specularGGX.ktx2", true);
        imageBuffers_["prefilteredMap"]->setSampler(samplerLinearRepeat_.handle());

        // Load irradiance map (cubemap for diffuse lighting)
        imageBuffers_["irradianceMap"]->createTextureFromKtx2(path + "diffuseLambertian.ktx2",
                                                              true);
        imageBuffers_["irradianceMap"]->setSampler(samplerLinearRepeat_.handle());

        // Load BRDF lookup table (2D texture)
        imageBuffers_["brdfLut"]->createTextureFromImage(path + "outputLUT.png", false, false);
        imageBuffers_["brdfLut"]->setSampler(samplerLinearClamp_.handle());
    }

    {
        TRACY_CPU_SCOPE("createHDRRenderTargets");
        // Create HDR render targets with selected format
        printLog("Creating HDR render targets:");
        printLog("  Format: {} ({} bytes/pixel)", vkFormatToString(selectedHDRFormat_),
                 getFormatSize(selectedHDRFormat_));

        // Log memory usage analysis
        logHDRMemoryUsage(swapchainWidth, swapchainHeight);

        // Storage color buffers for compute shaders and post-processing
        VkImageUsageFlags storageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                         VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
                                         VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                         VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        imageBuffers_["floatColor1"]->createImage(
            selectedHDRFormat_, swapchainWidth, swapchainHeight, VK_SAMPLE_COUNT_1_BIT,
            storageUsage, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, 0, VK_IMAGE_VIEW_TYPE_2D);

        imageBuffers_["floatColor2"]->createImage(
            selectedHDRFormat_, swapchainWidth, swapchainHeight, VK_SAMPLE_COUNT_1_BIT,
            storageUsage, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, 0, VK_IMAGE_VIEW_TYPE_2D);
    }

    {
        TRACY_CPU_SCOPE("createGBuffer");
        // Create G-buffer textures for deferred rendering
        printLog("Creating G-buffer textures for deferred rendering:");

        // G-Buffer format selection for optimal memory usage and precision
        VkFormat albedoFormat = VK_FORMAT_R8G8B8A8_UNORM; // Albedo + Metallic (4 bytes)
        VkFormat normalFormat =
            VK_FORMAT_R16G16B16A16_SFLOAT; // Normal + Roughness (8 bytes, needs precision)
        VkFormat positionFormat =
            VK_FORMAT_R32G32B32A32_SFLOAT; // Position + Depth (16 bytes, needs high precision)
        VkFormat materialFormat = VK_FORMAT_R8G8B8A8_UNORM; // AO + Emissive + Material ID (4 bytes)

        printLog("  gAlbedo: {} ({} bytes/pixel)", vkFormatToString(albedoFormat),
                 getFormatSize(albedoFormat));
        printLog("  gNormal: {} ({} bytes/pixel)", vkFormatToString(normalFormat),
                 getFormatSize(normalFormat));
        printLog("  gPosition: {} ({} bytes/pixel)", vkFormatToString(positionFormat),
                 getFormatSize(positionFormat));
        printLog("  gMaterial: {} ({} bytes/pixel)", vkFormatToString(materialFormat),
                 getFormatSize(materialFormat));

        // G-buffer usage flags (similar to floatColor but without storage bit since they're render
        // targets)
        VkImageUsageFlags gBufferUsage =
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        // Create gAlbedo buffer (Albedo RGB + Metallic A)
        imageBuffers_["gAlbedo"]->createImage(
            albedoFormat, swapchainWidth, swapchainHeight, VK_SAMPLE_COUNT_1_BIT, gBufferUsage,
            VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, 0, VK_IMAGE_VIEW_TYPE_2D);
        imageBuffers_["gAlbedo"]->setSampler(samplerLinearClamp_.handle());

        // Create gNormal buffer (World Normal RGB + Roughness A)
        imageBuffers_["gNormal"]->createImage(
            normalFormat, swapchainWidth, swapchainHeight, VK_SAMPLE_COUNT_1_BIT, gBufferUsage,
            VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, 0, VK_IMAGE_VIEW_TYPE_2D);
        imageBuffers_["gNormal"]->setSampler(samplerLinearClamp_.handle());

        // Create gPosition buffer (World Position RGB + Depth A)
        imageBuffers_["gPosition"]->createImage(
            positionFormat, swapchainWidth, swapchainHeight, VK_SAMPLE_COUNT_1_BIT, gBufferUsage,
            VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, 0, VK_IMAGE_VIEW_TYPE_2D);
        imageBuffers_["gPosition"]->setSampler(samplerLinearClamp_.handle());

        // Create gMaterial buffer (AO R + Emissive Intensity G + Material ID B + Unused A)
        imageBuffers_["gMaterial"]->createImage(
            materialFormat, swapchainWidth, swapchainHeight, VK_SAMPLE_COUNT_1_BIT, gBufferUsage,
            VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, 0, VK_IMAGE_VIEW_TYPE_2D);
        imageBuffers_["gMaterial"]->setSampler(samplerLinearClamp_.handle());

        printLog("G-buffer creation complete");
    }

    {
        TRACY_CPU_SCOPE("createDepthAndShadowBuffers");
        // Create depth buffer (no MSAA)
        imageBuffers_["depthStencil"]->createDepthBuffer(swapchainWidth, swapchainHeight);

        // Create shadow map
        uint32_t shadowMapSize = 2048 * 2;
        imageBuffers_["shadowMap"]->createShadow(shadowMapSize, shadowMapSize);
        imageBuffers_["shadowMap"]->setSampler(samplerShadow_.handle());
    }

    {
        TRACY_CPU_SCOPE("setSamplers");
        // Set samplers for storage and sampling images
        imageBuffers_["floatColor1"]->setSampler(samplerLinearRepeat_.handle());
        imageBuffers_["floatColor2"]->setSampler(samplerLinearRepeat_.handle());
        imageBuffers_["depthStencil"]->setSampler(samplerLinearClamp_.handle());
    }
}

// Format selection function with proper priority: float formats first, R8G8B8A8 last
VkFormat Renderer::selectOptimalHDRFormat(bool needsAlpha, bool fullPrecision)
{
    vector<VkFormat> candidateFormats;

    if (!needsAlpha && !fullPrecision) {
        // Memory-efficient HDR formats (no alpha, moderate precision)
        candidateFormats = {
            VK_FORMAT_B10G11R11_UFLOAT_PACK32, // 4 bytes - 50% savings, packed float (correct
                                               // format)
            VK_FORMAT_R16G16B16_SFLOAT,        // 6 bytes - 25% savings, half precision
            VK_FORMAT_R16G16B16A16_SFLOAT,     // 8 bytes - standard HDR with alpha
            VK_FORMAT_R32G32B32_SFLOAT,        // 12 bytes - full precision RGB
            // R8G8B8A8_UNORM is LAST - not a float format, poor for HDR
            VK_FORMAT_R8G8B8A8_UNORM // 4 bytes - NOT FLOAT, last resort
        };
    } else if (!fullPrecision) {
        // Standard HDR with alpha channel
        candidateFormats = {
            VK_FORMAT_R16G16B16A16_SFLOAT, // 8 bytes - standard HDR
            VK_FORMAT_R32G32B32A32_SFLOAT, // 16 bytes - full precision
            // R8G8B8A8_UNORM is LAST - not suitable for HDR
            VK_FORMAT_R8G8B8A8_UNORM // 4 bytes - NOT FLOAT, last resort
        };
    } else {
        // Full precision required
        candidateFormats = {
            VK_FORMAT_R32G32B32A32_SFLOAT, // 16 bytes - full precision
            VK_FORMAT_R32G32B32_SFLOAT,    // 12 bytes - full precision RGB
            VK_FORMAT_R16G16B16A16_SFLOAT, // 8 bytes - half precision fallback
            // R8G8B8A8_UNORM is LAST - inadequate for full precision HDR
            VK_FORMAT_R8G8B8A8_UNORM // 4 bytes - NOT FLOAT, emergency fallback
        };
    }

    // Test each format for compatibility (float formats first, R8G8B8A8 last)
    for (size_t i = 0; i < candidateFormats.size(); ++i) {
        VkFormat format = candidateFormats[i];

        if (isFormatSuitableForHDR(format)) {
            string formatType = (format == VK_FORMAT_R8G8B8A8_UNORM) ? "NON-FLOAT" : "FLOAT";
            float memoryRatio = static_cast<float>(getFormatSize(format)) / 8.0f; // vs RGBA16F

            printLog("✓ Selected HDR format: {} ({} bytes/pixel, {}, {:.0f}% memory vs RGBA16F)",
                     vkFormatToString(format), getFormatSize(format), formatType,
                     memoryRatio * 100.0f);

            // Warn if we fell back to non-float format
            if (format == VK_FORMAT_R8G8B8A8_UNORM) {
                printLog("⚠ WARNING: Using R8G8B8A8_UNORM for HDR - limited dynamic range!");
                printLog("  Consider using float formats for better HDR quality");
            }

            return format;
        } else {
            string formatType = (format == VK_FORMAT_R8G8B8A8_UNORM) ? "NON-FLOAT" : "FLOAT";
            printLog("✗ Format {} ({}) not supported, trying next...", vkFormatToString(format),
                     formatType);
        }
    }

    // Emergency fallback - this should rarely happen
    printLog(
        "⚠ All candidate formats failed, using emergency fallback: VK_FORMAT_R16G16B16A16_SFLOAT");
    return VK_FORMAT_R16G16B16A16_SFLOAT;
}

// Enhanced format validation function
bool Renderer::isFormatSuitableForHDR(VkFormat format)
{
    // Check if format supports required features for HDR rendering
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(ctx_.physicalDevice(), format, &props);

    // Required features for HDR color attachments
    VkFormatFeatureFlags requiredFeatures =
        VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | // Can render to it
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;     // Can sample from it

    // Optional but preferred for HDR
    VkFormatFeatureFlags preferredFeatures =
        VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT; // Can blend (for transparency)

    bool hasRequired = (props.optimalTilingFeatures & requiredFeatures) == requiredFeatures;
    bool hasPreferred = (props.optimalTilingFeatures & preferredFeatures) == preferredFeatures;

    if (hasRequired && !hasPreferred && format != VK_FORMAT_R8G8B8A8_UNORM) {
        printLog("  Note: {} missing blend support but acceptable for HDR",
                 vkFormatToString(format));
    }

    return hasRequired;
}

void Renderer::logHDRMemoryUsage(uint32_t width, uint32_t height)
{
    uint64_t totalPixels = static_cast<uint64_t>(width) * height;

    uint32_t hdrBytes = getFormatSize(selectedHDRFormat_);
    uint32_t standardBytes = getFormatSize(VK_FORMAT_R16G16B16A16_SFLOAT);

    // Calculate memory usage for current format (no MSAA, so 1x samples)
    uint64_t hdrMemoryMB = (totalPixels * hdrBytes + totalPixels * hdrBytes * 2) / (1024 * 1024);
    uint64_t standardMemoryMB =
        (totalPixels * standardBytes + totalPixels * standardBytes * 2) / (1024 * 1024);

    float savings =
        (1.0f - static_cast<float>(hdrMemoryMB) / static_cast<float>(standardMemoryMB)) * 100.0f;

    printLog("HDR Memory Analysis:");
    printLog("  Resolution: {}x{} (no MSAA)", width, height);
    printLog("  Current format memory: {} MB", hdrMemoryMB);
    printLog("  Standard RGBA16F memory: {} MB", standardMemoryMB);
    if (savings > 0) {
        printLog("  Memory savings: {:.1f}%", savings);
    } else {
        printLog("  Memory overhead: {:.1f}%", -savings);
    }

    // Quality assessment
    if (selectedHDRFormat_ == VK_FORMAT_R8G8B8A8_UNORM) {
        printLog("  Quality: ⚠ LIMITED - R8G8B8A8 has restricted HDR range");
    } else if (selectedHDRFormat_ == VK_FORMAT_B10G11R11_UFLOAT_PACK32) {
        printLog("  Quality: ✓ GOOD - B10G11R11 excellent for HDR with memory savings");
    } else if (selectedHDRFormat_ == VK_FORMAT_R16G16B16A16_SFLOAT) {
        printLog("  Quality: ✓ EXCELLENT - Standard HDR format");
    } else {
        printLog("  Quality: ✓ HIGH - Float format suitable for HDR");
    }
}

void Renderer::updateViewFrustum(const glm::mat4& viewProjection)
{
    TRACY_CPU_SCOPE("updateViewFrustum");

    if (frustumCullingEnabled_) {
        viewFrustum_.extractFromViewProjection(viewProjection);
    }
}

void Renderer::performFrustumCulling(vector<unique_ptr<Model>>& models)
{
    TRACY_CPU_SCOPE("performFrustumCulling");

    cullingStats_.totalMeshes = 0;
    cullingStats_.culledMeshes = 0;
    cullingStats_.renderedMeshes = 0;

    if (!frustumCullingEnabled_) {
        for (auto& model : models) {
            for (auto& mesh : model->meshes()) {
                mesh.isCulled = false;
                cullingStats_.totalMeshes++;
                cullingStats_.renderedMeshes++;
            }
        }
        return;
    }

    {
        TRACY_CPU_SCOPE("frustumCullingLoop");
        for (auto& model : models) {
            for (auto& mesh : model->meshes()) {
                cullingStats_.totalMeshes++;

                bool isVisible = viewFrustum_.intersects(mesh.worldBounds);

                mesh.isCulled = !isVisible;

                if (isVisible) {
                    cullingStats_.renderedMeshes++;
                } else {
                    cullingStats_.culledMeshes++;
                }
            }
        }
    }

    // Track culling statistics in Tracy
    TRACY_PLOT("FrustumCulling_TotalMeshes", static_cast<int64_t>(cullingStats_.totalMeshes));
    TRACY_PLOT("FrustumCulling_RenderedMeshes", static_cast<int64_t>(cullingStats_.renderedMeshes));
    TRACY_PLOT("FrustumCulling_CulledMeshes", static_cast<int64_t>(cullingStats_.culledMeshes));

    if (cullingStats_.totalMeshes > 0) {
        float cullingEfficiency =
            (float(cullingStats_.culledMeshes) / float(cullingStats_.totalMeshes)) * 100.0f;
        TRACY_PLOT("FrustumCulling_EfficiencyPercent", static_cast<int64_t>(cullingEfficiency));
    }
}

void Renderer::updateWorldBounds(vector<unique_ptr<Model>>& models)
{
    TRACY_CPU_SCOPE("updateWorldBounds");

    for (auto& model : models) {
        for (auto& mesh : model->meshes()) {
            mesh.updateWorldBounds(model->modelMatrix());
        }
    }
}

void Renderer::setFrustumCullingEnabled(bool enabled)
{
    frustumCullingEnabled_ = enabled;
}

bool Renderer::isFrustumCullingEnabled() const
{
    return frustumCullingEnabled_;
}

const CullingStats& Renderer::getCullingStats() const
{
    return cullingStats_;
}

VkRenderingAttachmentInfo Renderer::createColorAttachment(VkImageView imageView,
                                                          VkAttachmentLoadOp loadOp,
                                                          VkClearColorValue clearColor,
                                                          VkImageView resolveImageView,
                                                          VkResolveModeFlagBits resolveMode) const
{
    VkRenderingAttachmentInfo attachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    attachment.imageView = imageView;
    attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment.loadOp = loadOp;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.clearValue.color = clearColor;
    attachment.resolveMode = resolveMode;
    attachment.resolveImageView = resolveImageView;
    attachment.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    return attachment;
}

VkRenderingAttachmentInfo Renderer::createDepthAttachment(VkImageView imageView,
                                                          VkAttachmentLoadOp loadOp,
                                                          float clearDepth,
                                                          VkImageView resolveImageView,
                                                          VkResolveModeFlagBits resolveMode) const
{
    VkRenderingAttachmentInfo attachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    attachment.imageView = imageView;
    attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachment.loadOp = loadOp;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.clearValue.depthStencil = {clearDepth, 0};
    attachment.resolveMode = resolveMode;
    attachment.resolveImageView = resolveImageView;
    attachment.resolveImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    return attachment;
}

} // namespace hlab