#include "Renderer.h"
#include <stb_image.h>

namespace hlab {

Renderer::Renderer(Context& ctx, ShaderManager& shaderManager, const uint32_t& kMaxFramesInFlight,
                   const string& kAssetsPathPrefix, const string& kShaderPathPrefix_)
    : ctx_(ctx), shaderManager_(shaderManager), kMaxFramesInFlight_(kMaxFramesInFlight),
      kAssetsPathPrefix_(kAssetsPathPrefix), kShaderPathPrefix_(kShaderPathPrefix_),
      skyTextures_(ctx), samplerShadow_(ctx), samplerLinearRepeat_(ctx), samplerLinearClamp_(ctx),
      samplerAnisoRepeat_(ctx), samplerAnisoClamp_(ctx), textureManager_(ctx),
      materialStorageBuffer_(ctx)
{
}

void Renderer::prepareForModels(vector<unique_ptr<Model>>& models, VkFormat outColorFormat,
                                VkFormat depthFormat, VkSampleCountFlagBits msaaSamples,
                                uint32_t swapChainWidth, uint32_t swapChainHeight)
{
    createPipelines(outColorFormat, depthFormat, msaaSamples);
    createTextures(swapChainWidth, swapChainHeight, msaaSamples);
    createUniformBuffers();

    vector<MaterialUBO> allMaterials;

    for (auto& m : models) {
        m->createDescriptorSets(samplerLinearRepeat_, allMaterials, textureManager_);
    }

    VkDeviceSize size = sizeof(MaterialUBO) * allMaterials.size();
    materialStorageBuffer_.create(size);
    materialStorageBuffer_.copyData(allMaterials.data(), size);

    // Create single descriptor set for all materials
    materialDescriptorSet_.create(ctx_, {materialStorageBuffer_, textureManager_});
}

void Renderer::createUniformBuffers()
{
    const VkDevice device = ctx_.device();

    // Initialize uniform buffer map with proper keys
    const vector<string> bufferNames = {"sceneData", "skyOptions",  "options",
                                        "boneData",  "postOptions", "ssaoOptions"};

    for (const auto& name : bufferNames) {
        uniformBuffers_[name].clear();
        uniformBuffers_[name].reserve(kMaxFramesInFlight_);
    }

    // Create uniform buffers for each type and frame
    for (uint32_t i = 0; i < kMaxFramesInFlight_; ++i) {
        // Scene uniform buffers
        auto sceneBuffer = make_unique<MappedBuffer>(ctx_);
        sceneBuffer->createUniformBuffer(sceneUBO_);
        uniformBuffers_["sceneData"].emplace_back(std::move(sceneBuffer));

        // Options uniform buffers
        auto optionsBuffer = make_unique<MappedBuffer>(ctx_);
        optionsBuffer->createUniformBuffer(optionsUBO_);
        uniformBuffers_["options"].emplace_back(std::move(optionsBuffer));

        // Sky options uniform buffers
        auto skyOptionsBuffer = make_unique<MappedBuffer>(ctx_);
        skyOptionsBuffer->createUniformBuffer(skyOptionsUBO_);
        uniformBuffers_["skyOptions"].emplace_back(std::move(skyOptionsBuffer));

        // Post options uniform buffers
        auto postOptionsBuffer = make_unique<MappedBuffer>(ctx_);
        postOptionsBuffer->createUniformBuffer(postOptionsUBO_);
        uniformBuffers_["postOptions"].emplace_back(std::move(postOptionsBuffer));

        // SSAO options uniform buffers
        auto ssaoOptionsBuffer = make_unique<MappedBuffer>(ctx_);
        ssaoOptionsBuffer->createUniformBuffer(ssaoOptionsUBO_);
        uniformBuffers_["ssaoOptions"].emplace_back(std::move(ssaoOptionsBuffer));

        // Bone data uniform buffers
        auto boneDataBuffer = make_unique<MappedBuffer>(ctx_);
        boneDataBuffer->createUniformBuffer(boneDataUBO_);
        uniformBuffers_["boneData"].emplace_back(std::move(boneDataBuffer));
    }

    // Create descriptor sets using the new uniform buffer structure
    sceneOptionsBoneDataSets_.resize(kMaxFramesInFlight_);
    for (size_t i = 0; i < kMaxFramesInFlight_; i++) {
        sceneOptionsBoneDataSets_[i].create(ctx_, {*uniformBuffers_["sceneData"][i],
                                                   *uniformBuffers_["options"][i],
                                                   *uniformBuffers_["boneData"][i]});
    }

    sceneSkyOptionsSets_.resize(kMaxFramesInFlight_);
    for (size_t i = 0; i < kMaxFramesInFlight_; i++) {
        sceneSkyOptionsSets_[i].create(
            ctx_, {*uniformBuffers_["sceneData"][i], *uniformBuffers_["skyOptions"][i]});
    }

    postProcessingDescriptorSets_.resize(kMaxFramesInFlight_);
    for (size_t i = 0; i < kMaxFramesInFlight_; i++) {
        postProcessingDescriptorSets_[i].create(
            ctx_, {*computeToPost_, *uniformBuffers_["postOptions"][i]});
    }

    // Create SSAO descriptor sets
    // Ensure forwardToCompute_ and computeToPost_ are properly configured for storage binding
    // forwardToCompute_ will be used as readonly storage image (input)
    // computeToPost_ will be used as writeonly storage image (output)
    // Create a command buffer for image layout transitions
    auto cmd = ctx_.createGraphicsCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    forwardToCompute_->transitionToGeneral(cmd.handle(), VK_ACCESS_2_SHADER_READ_BIT,
                                           VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
    computeToPost_->transitionToGeneral(cmd.handle(), VK_ACCESS_2_SHADER_WRITE_BIT,
                                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
    cmd.submitAndWait();
    ssaoDescriptorSets_.resize(kMaxFramesInFlight_);
    for (size_t i = 0; i < kMaxFramesInFlight_; i++) {
        ssaoDescriptorSets_[i].create(ctx_, {*uniformBuffers_["sceneData"][i],
                                             *uniformBuffers_["ssaoOptions"][i], *forwardToCompute_,
                                             *computeToPost_, *depthStencil_});
    }
}

void Renderer::update(Camera& camera, vector<unique_ptr<Model>>& models, uint32_t currentFrame,
                      double time)
{
    // Update view frustum based on current camera view-projection matrix
    updateViewFrustum(camera.matrices.perspective * camera.matrices.view);
    updateWorldBounds(models);
    updateBoneData(models, currentFrame);
    performFrustumCulling(models);

    // Update all uniform buffers using direct iteration over the map
    for (const auto& [bufferName, bufferVector] : uniformBuffers_) {
        bufferVector[currentFrame]->updateFromCpuData();
    }
}

void Renderer::updateBoneData(const vector<unique_ptr<Model>>& models, uint32_t currentFrame)
{
    // Reset bone data
    boneDataUBO_.animationData.x = 0.0f;
    for (int i = 0; i < 256; ++i) {
        boneDataUBO_.boneMatrices[i] = glm::mat4(1.0f);
    }

    // Check if any model has animation data
    bool hasAnyAnimation = false;
    for (const auto& model : models) {
        if (model->hasAnimations() && model->hasBones()) {
            hasAnyAnimation = true;

            // Get bone matrices from the first animated model
            const auto& boneMatrices = model->getBoneMatrices();

            // Copy bone matrices (up to 256 bones)
            const size_t maxBones = 256;
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
}

void Renderer::draw(VkCommandBuffer cmd, uint32_t currentFrame, VkImageView swapchainImageView,
                    vector<unique_ptr<Model>>& models, VkViewport viewport, VkRect2D scissor)
{
    VkRect2D renderArea = {0, 0, scissor.extent.width, scissor.extent.height};

    // Forward rendering pass
    {
        forwardToCompute_->resourceBinding().barrierHelper().transitionTo(
            cmd, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

        auto colorAttachment = createColorAttachment(
            msaaColorBuffer_->view(), VK_ATTACHMENT_LOAD_OP_CLEAR, {0.0f, 0.0f, 0.5f, 0.0f},
            forwardToCompute_->view(), VK_RESOLVE_MODE_AVERAGE_BIT);
        auto depthAttachment = createDepthAttachment(
            msaaDepthStencil_->attachmentView(), VK_ATTACHMENT_LOAD_OP_CLEAR, 1.0f,
            depthStencil_->attachmentView(), VK_RESOLVE_MODE_SAMPLE_ZERO_BIT);
        auto renderingInfo = createRenderingInfo(renderArea, &colorAttachment, &depthAttachment);

        vkCmdBeginRendering(cmd, &renderingInfo);
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        VkDeviceSize offsets[1]{0};

        // Render models
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipelines_.at("pbrForward").pipeline());

        // Bind descriptor sets once per model (no longer per material)
        const auto descriptorSets = vector{
            sceneOptionsBoneDataSets_[currentFrame].handle(), // Set 0: scene, options, bone data
            materialDescriptorSet_.handle(), // Set 1: material storage buffer + textures
            skyDescriptorSet_.handle(),      // Set 2: sky textures
            shadowMapSet_.handle()           // Set 3: shadow map
        };

        vkCmdBindDescriptorSets(
            cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines_.at("pbrForward").pipelineLayout(), 0,
            static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(), 0, nullptr);

        for (size_t j = 0; j < models.size(); j++) {
            if (!models[j]->visible()) {
                continue;
            }

            for (size_t i = 0; i < models[j]->meshes().size(); i++) {
                auto& mesh = models[j]->meshes()[i];

                // Skip culled meshes
                if (mesh.isCulled) {
                    continue;
                }

                uint32_t matIndex = mesh.materialIndex_;

                // Create push constants with material index
                PbrPushConstants pushConstants;
                pushConstants.model = models[j]->modelMatrix();
                pushConstants.materialIndex = matIndex;

                // Copy existing coeffs (reduced to 15)
                for (int k = 0; k < 15; ++k) {
                    pushConstants.coeffs[k] = models[j]->coeffs()[k];
                }

                // Push constants with material index
                vkCmdPushConstants(cmd, pipelines_.at("pbrForward").pipelineLayout(),
                                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                   sizeof(PbrPushConstants), &pushConstants);

                vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.vertexBuffer_, offsets);
                vkCmdBindIndexBuffer(cmd, mesh.indexBuffer_, 0, VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(cmd, static_cast<uint32_t>(mesh.indices_.size()), 1, 0, 0, 0);
            }
        }

        // Sky rendering pass
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines_.at("sky").pipeline());

        const auto skyDescriptorSets = vector{
            sceneSkyOptionsSets_[currentFrame].handle(), // Set 0: scene + sky options
            skyDescriptorSet_.handle()                   // Set 1: sky textures
        };

        vkCmdBindDescriptorSets(
            cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines_.at("sky").pipelineLayout(), 0,
            static_cast<uint32_t>(skyDescriptorSets.size()), skyDescriptorSets.data(), 0, nullptr);
        vkCmdDraw(cmd, 36, 1, 0, 0);
        vkCmdEndRendering(cmd);
    }

    // SSAO
    {
        // Transition images for SSAO compute pass to proper storage layouts
        // forwardToCompute_: Forward rendering result → readonly storage image for SSAO input
        forwardToCompute_->transitionToGeneral(cmd, VK_ACCESS_2_SHADER_READ_BIT,
                                               VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

        // computeToPost_: Empty buffer → writeonly storage image for SSAO output
        computeToPost_->transitionToGeneral(cmd, VK_ACCESS_2_SHADER_WRITE_BIT,
                                            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

        // depthStencil_: Depth buffer → sampled texture for depth-based calculations
        depthStencil_->transitionToShaderRead(cmd);

        // Bind SSAO compute pipeline
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines_.at("ssao").pipeline());

        // Bind descriptor sets for SSAO
        const auto ssaoDescriptorSets = vector{ssaoDescriptorSets_[currentFrame].handle()};
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                pipelines_.at("ssao").pipelineLayout(), 0,
                                static_cast<uint32_t>(ssaoDescriptorSets.size()),
                                ssaoDescriptorSets.data(), 0, nullptr);

        // Dispatch compute shader
        // Calculate dispatch size based on image dimensions and local work group size (16x16)
        uint32_t groupCountX = (scissor.extent.width + 15) / 16;  // Round up division
        uint32_t groupCountY = (scissor.extent.height + 15) / 16; // Round up division
        vkCmdDispatch(cmd, groupCountX, groupCountY, 1);

        // Memory barrier to ensure compute writes are visible to fragment shader
        VkMemoryBarrier2 memoryBarrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
        memoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        memoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        memoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

        VkDependencyInfo dependencyInfo{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        dependencyInfo.memoryBarrierCount = 1;
        dependencyInfo.pMemoryBarriers = &memoryBarrier;
        vkCmdPipelineBarrier2(cmd, &dependencyInfo);
    }

    // Post-processing pass
    {
        computeToPost_->transitionToShaderRead(cmd);

        auto colorAttachment = createColorAttachment(
            swapchainImageView, VK_ATTACHMENT_LOAD_OP_CLEAR, {0.0f, 0.0f, 1.0f, 0.0f});

        // No depth attachment needed for post-processing
        auto renderingInfo = createRenderingInfo(renderArea, &colorAttachment, nullptr);

        vkCmdBeginRendering(cmd, &renderingInfo);
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines_.at("post").pipeline());

        const auto postDescriptorSets =
            vector{postProcessingDescriptorSets_[currentFrame].handle()};
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipelines_.at("post").pipelineLayout(), 0,
                                static_cast<uint32_t>(postDescriptorSets.size()),
                                postDescriptorSets.data(), 0, nullptr);

        vkCmdDraw(cmd, 6, 1, 0, 0);
        vkCmdEndRendering(cmd);
    }
}

void Renderer::makeShadowMap(VkCommandBuffer cmd, uint32_t currentFrame,
                             vector<unique_ptr<Model>>& models)
{
    // Transition shadow map image to depth-stencil attachment layout
    shadowMap_->transitionToDepthStencilAttachment(cmd);

    VkRenderingAttachmentInfo shadowDepthAttachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    shadowDepthAttachment.imageView = shadowMap_->view();
    shadowDepthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    shadowDepthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    shadowDepthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    shadowDepthAttachment.clearValue.depthStencil = {1.0f, 0};

    VkRenderingInfo shadowRenderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO_KHR};
    shadowRenderingInfo.renderArea = {0, 0, shadowMap_->width(), shadowMap_->height()};
    shadowRenderingInfo.layerCount = 1;
    shadowRenderingInfo.colorAttachmentCount = 0;
    shadowRenderingInfo.pDepthAttachment = &shadowDepthAttachment;

    VkViewport shadowViewport{0.0f, 0.0f, (float)shadowMap_->width(), (float)shadowMap_->height(),
                              0.0f, 1.0f};
    VkRect2D shadowScissor{0, 0, shadowMap_->width(), shadowMap_->height()};

    vkCmdBeginRendering(cmd, &shadowRenderingInfo);
    vkCmdSetViewport(cmd, 0, 1, &shadowViewport);
    vkCmdSetScissor(cmd, 0, 1, &shadowScissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines_.at("shadowMap").pipeline());

    const auto descriptorSets = vector{sceneOptionsBoneDataSets_[currentFrame].handle()};

    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines_.at("shadowMap").pipelineLayout(), 0,
        static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(), 0, nullptr);

    vkCmdSetDepthBias(cmd,
                      1.1f,  // Constant factor
                      0.0f,  // Clamp value
                      2.0f); // Slope factor

    // Render all visible models to shadow map
    VkDeviceSize offsets[1]{0};

    for (size_t j = 0; j < models.size(); j++) {
        if (!models[j]->visible()) {
            continue;
        }

        vkCmdPushConstants(cmd, pipelines_.at("shadowMap").pipelineLayout(),
                           VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(models[j]->modelMatrix()),
                           &models[j]->modelMatrix());

        // Render all meshes in this model
        for (size_t i = 0; i < models[j]->meshes().size(); i++) {
            auto& mesh = models[j]->meshes()[i];

            // Skip culled meshes for shadow mapping too
            if (mesh.isCulled) {
                continue;
            }

            // Bind vertex and index buffers
            vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.vertexBuffer_, offsets);
            vkCmdBindIndexBuffer(cmd, mesh.indexBuffer_, 0, VK_INDEX_TYPE_UINT32);

            // Draw the mesh
            vkCmdDrawIndexed(cmd, static_cast<uint32_t>(mesh.indices_.size()), 1, 0, 0, 0);
        }
    }

    vkCmdEndRendering(cmd);

    // Transition shadow map to shader read-only for sampling in main render pass
    shadowMap_->transitionToShaderRead(cmd);
}

void Renderer::createPipelines(const VkFormat swapChainColorFormat, const VkFormat depthFormat,
                               VkSampleCountFlagBits msaaSamples)
{
    // NEW: Using PipelineConfig-based creation
    pipelines_.emplace("pbrForward",
                       Pipeline(ctx_, shaderManager_, PipelineConfig::createPbrForward(),
                                VK_FORMAT_R16G16B16A16_SFLOAT, depthFormat, msaaSamples));

    pipelines_.emplace("sky", Pipeline(ctx_, shaderManager_, PipelineConfig::createSky(),
                                       VK_FORMAT_R16G16B16A16_SFLOAT, depthFormat, msaaSamples));

    pipelines_.emplace("post", Pipeline(ctx_, shaderManager_, PipelineConfig::createPost(),
                                        swapChainColorFormat, depthFormat, VK_SAMPLE_COUNT_1_BIT));

    pipelines_.emplace("shadowMap",
                       Pipeline(ctx_, shaderManager_, PipelineConfig::createShadowMap(),
                                VK_FORMAT_D16_UNORM, VK_FORMAT_D16_UNORM, VK_SAMPLE_COUNT_1_BIT));

    pipelines_.emplace("ssao",
                       Pipeline(ctx_, shaderManager_, PipelineConfig::createSsao(),
                                VK_FORMAT_D16_UNORM, VK_FORMAT_D16_UNORM, VK_SAMPLE_COUNT_1_BIT));
}

void Renderer::createTextures(uint32_t swapchainWidth, uint32_t swapchainHeight,
                              VkSampleCountFlagBits msaaSamples)
{
    samplerLinearRepeat_.createLinearRepeat();
    samplerLinearClamp_.createLinearClamp();
    samplerAnisoRepeat_.createAnisoRepeat();
    samplerAnisoClamp_.createAnisoClamp();
    samplerShadow_.createShadow();

    // Create Image2D objects here when dimensions are known
    dummyTexture_ = make_unique<Image2D>(ctx_);
    msaaColorBuffer_ = make_unique<Image2D>(ctx_);
    depthStencil_ = make_unique<Image2D>(ctx_);
    msaaDepthStencil_ = make_unique<Image2D>(ctx_);
    forwardToCompute_ = make_unique<Image2D>(ctx_);
    computeToPost_ = make_unique<Image2D>(ctx_);
    shadowMap_ = make_unique<Image2D>(ctx_);

    string dummyImagePath = kAssetsPathPrefix_ + "textures/blender_uv_grid_2k.png";

    // Load texture from file using stb_image
    {
        int width, height, channels;
        unsigned char* pixels =
            stbi_load(dummyImagePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);

        if (!pixels) {
            exitWithMessage("Failed to load texture image: {}", dummyImagePath);
        }

        dummyTexture_->createFromPixelData(pixels, width, height, channels, true);

        // Free the loaded image data (only if we loaded from file)
        if (pixels != nullptr && dummyImagePath.find(".png") != string::npos) {
            stbi_image_free(pixels);
        }

        dummyTexture_->setSampler(samplerLinearRepeat_.handle());
    }

    // Initialize IBL textures for PBR
    string path = kAssetsPathPrefix_ + "textures/golden_gate_hills_4k/";
    skyTextures_.loadKtxMaps(path + "specularGGX.ktx2", path + "diffuseLambertian.ktx2",
                             path + "outputLUT.png");

    // Create render targets
    msaaColorBuffer_->createMsaaColorBuffer(swapchainWidth, swapchainHeight, msaaSamples);
    msaaDepthStencil_->createDepthBuffer(swapchainWidth, swapchainHeight, msaaSamples);
    depthStencil_->createDepthBuffer(swapchainWidth, swapchainHeight, VK_SAMPLE_COUNT_1_BIT);
    forwardToCompute_->createGeneralStorage(swapchainWidth, swapchainHeight);
    computeToPost_->createGeneralStorage(swapchainWidth, swapchainHeight);

    // Create shadow map using Image2D
    uint32_t shadowMapSize = 2048 * 2;
    shadowMap_->createShadow(shadowMapSize, shadowMapSize);
    shadowMap_->setSampler(samplerShadow_.handle());

    // Set samplers
    forwardToCompute_->setSampler(samplerLinearRepeat_.handle());
    computeToPost_->setSampler(samplerLinearRepeat_.handle());
    depthStencil_->setSampler(samplerLinearClamp_.handle());

    // Create descriptor sets for sky textures (set 1 for sky pipeline)
    skyDescriptorSet_.create(
        ctx_, {skyTextures_.prefiltered(), skyTextures_.irradiance(), skyTextures_.brdfLUT()});

    // Create descriptor set for shadow mapping
    shadowMapSet_.create(ctx_, {*shadowMap_});
}

void Renderer::updateViewFrustum(const glm::mat4& viewProjection)
{
    if (frustumCullingEnabled_) {
        viewFrustum_.extractFromViewProjection(viewProjection);
    }
}

// Add overload for all models
void Renderer::performFrustumCulling(vector<unique_ptr<Model>>& models)
{
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

void Renderer::updateWorldBounds(vector<unique_ptr<Model>>& models)
{
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

VkRenderingInfo
Renderer::createRenderingInfo(const VkRect2D& renderArea,
                              const VkRenderingAttachmentInfo* colorAttachment,
                              const VkRenderingAttachmentInfo* depthAttachment) const
{
    VkRenderingInfo renderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO_KHR};
    renderingInfo.renderArea = renderArea;
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = colorAttachment ? 1 : 0;
    renderingInfo.pColorAttachments = colorAttachment;
    renderingInfo.pDepthAttachment = depthAttachment;
    renderingInfo.pStencilAttachment = depthAttachment;
    return renderingInfo;
}

} // namespace hlab