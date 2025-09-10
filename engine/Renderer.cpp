#include "Renderer.h"
#include "Logger.h"
#include <stb_image.h>

namespace hlab {

Renderer::Renderer(Context& ctx, ShaderManager& shaderManager, const uint32_t& kMaxFramesInFlight,
                   const string& kAssetsPathPrefix, const string& kShaderPathPrefix_,
                   vector<unique_ptr<Model>>& models, VkFormat outColorFormat, VkFormat depthFormat,
                   VkSampleCountFlagBits msaaSamples, uint32_t swapChainWidth,
                   uint32_t swapChainHeight)
    : ctx_(ctx), shaderManager_(shaderManager), kMaxFramesInFlight_(kMaxFramesInFlight),
      kAssetsPathPrefix_(kAssetsPathPrefix), kShaderPathPrefix_(kShaderPathPrefix_),
      samplerShadow_(ctx), samplerLinearRepeat_(ctx), samplerLinearClamp_(ctx),
      samplerAnisoRepeat_(ctx), samplerAnisoClamp_(ctx),
      materialTextures_(make_unique<TextureManager>(ctx))
{
    createPipelines(outColorFormat, depthFormat, msaaSamples);
    createTextures(swapChainWidth, swapChainHeight, msaaSamples);
    createUniformBuffers();

    vector<MaterialUBO> allMaterials;

    for (auto& m : models) {
        m->prepareForBindlessRendering(samplerLinearRepeat_, allMaterials, *materialTextures_);
    }

    materialBuffer_ = make_unique<StorageBuffer>(ctx_, allMaterials.data(),
                                                 sizeof(MaterialUBO) * allMaterials.size());

    unordered_map<string, vector<string>> descriptorSetNames; // TODO: move to script
    descriptorSetNames["shadowMap"] = {"sceneOptions"};
    descriptorSetNames["pbrForward"] = {"sceneOptions", "material", "sky", "shadowMap"};
    descriptorSetNames["sky"] = {"skyOptions", "sky"};
    descriptorSetNames["ssao"] = {"ssao"};
    descriptorSetNames["post"] = {"postProcessing"};

    unordered_map<string, vector<vector<BindingInfo>>> bindingInfos = shaderManager_.bindingInfos();

    for (auto i : descriptorSetNames) {
        auto pipelineName = i.first;

        cout << pipelineName << endl;

        auto& bindings = bindingInfos.at(pipelineName);

        assert(bindings.size() == descriptorSetNames[pipelineName].size());

        for (int s = 0; s < bindings.size(); s++) {

            string setName = descriptorSetNames[pipelineName][s];

            if (perFrameDescriptorSets_.find(setName) != perFrameDescriptorSets_.end())
                continue;
            if (descriptorSets_.find(setName) != descriptorSets_.end())
                continue;

            cout << "Set " << s << " " << setName << endl;

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
    // Update view frustum based on current camera view-projection matrix
    updateViewFrustum(camera.matrices.perspective * camera.matrices.view);
    updateWorldBounds(models);
    updateBoneData(models, currentFrame);
    performFrustumCulling(models);

    // Update all uniform buffers using direct iteration over the map
    for (const auto& [bufferName, bufferVector] : perFrameUniformBuffers_) {
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

    // Update the GPU buffer using the consolidated map structure
    perFrameUniformBuffers_["boneData"][currentFrame]->updateFromCpuData();
}

void Renderer::draw(VkCommandBuffer cmd, uint32_t currentFrame, VkImageView swapchainImageView,
                    vector<unique_ptr<Model>>& models, VkViewport viewport, VkRect2D scissor)
{
    for (auto& renderNode : renderGraph_.renderNodes_) {
        if (renderNode.pipelineNames[0] == "ssao") {
            pipelines_.at("ssao")->dispatch(cmd, currentFrame); // Compute
            continue;
        }

        string mainTarget;
        vector<VkRenderingAttachmentInfo> colorAttachments{};
        VkRenderingAttachmentInfo depthAttachment{};
        VkRenderingAttachmentInfo stencilAttachment{};
        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;

        if (renderNode.msaaColorAttachments.size() > 0) {
            for (uint32_t i = 0; i < renderNode.msaaColorAttachments.size(); i++) {
                if (mainTarget.empty()) {
                    mainTarget = renderNode.msaaColorAttachments[i];
                }
                colorAttachments.push_back(
                    createColorAttachment(imageBuffers_[renderNode.msaaColorAttachments[i]]->view(),
                                          VK_ATTACHMENT_LOAD_OP_CLEAR, {0.0f, 0.0f, 0.5f, 0.0f},
                                          imageBuffers_[renderNode.colorAttachments[i]]->view(),
                                          VK_RESOLVE_MODE_AVERAGE_BIT));
            }

            depthAttachment = createDepthAttachment(
                imageBuffers_[renderNode.msaaDepthAttachment]->attachmentView(),
                VK_ATTACHMENT_LOAD_OP_CLEAR, 1.0f,
                imageBuffers_[renderNode.depthAttachment]->attachmentView(),
                VK_RESOLVE_MODE_SAMPLE_ZERO_BIT);

            renderingInfo.pDepthAttachment = &depthAttachment;

            if (!renderNode.msaaStencilAttachment.empty()) {
                renderingInfo.pStencilAttachment = &depthAttachment;
            }
        } else {

            for (const auto c : renderNode.colorAttachments) {
                if (c == "swapchain") {
                    colorAttachments.push_back(createColorAttachment(
                        swapchainImageView, VK_ATTACHMENT_LOAD_OP_CLEAR, {0.0f, 0.0f, 1.0f, 0.0f}));
                }
            }

            if (!renderNode.depthAttachment.empty()) {
                if (mainTarget.empty()) {
                    mainTarget = renderNode.depthAttachment;
                }
                imageBuffers_[renderNode.depthAttachment]->transitionToDepthStencilAttachment(cmd);
                depthAttachment =
                    createDepthAttachment(imageBuffers_[renderNode.depthAttachment]->view(),
                                          VK_ATTACHMENT_LOAD_OP_CLEAR, 1.0f);
                renderingInfo.pDepthAttachment = &depthAttachment;
            }
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

        for (auto& pipelineName : renderNode.pipelineNames) {

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipelines_.at(pipelineName)->pipeline());

            pipelines_.at(pipelineName)->bindDescriptorSets(cmd, currentFrame);

            if (pipelineName == "sky") {
                vkCmdDraw(cmd, 36, 1, 0, 0);
                continue;
            }

            if (pipelineName == "post") {
                vkCmdDraw(cmd, 6, 1, 0, 0);
                continue;
            }

            if (pipelineName == "shadowMap") { // 파이프라인 옵션 추가
                vkCmdSetDepthBias(cmd,
                                  1.1f,  // Constant factor
                                  0.0f,  // Clamp value
                                  2.0f); // Slope factor
            }

            // Render all visible models to shadow map
            VkDeviceSize offsets[1]{0};

            for (size_t j = 0; j < models.size(); j++) {
                if (!models[j]->visible()) {
                    continue;
                }

                // Render all meshes in this model
                for (size_t i = 0; i < models[j]->meshes().size(); i++) {
                    auto& mesh = models[j]->meshes()[i];

                    // Skip culled meshes for shadow mapping too
                    if (mesh.isCulled) {
                        continue;
                    }

                    PbrPushConstants pushConstants;
                    pushConstants.model = models[j]->modelMatrix();
                    pushConstants.materialIndex = mesh.materialIndex_;
                    vkCmdPushConstants(cmd, pipelines_.at(pipelineName)->pipelineLayout(),
                                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                       sizeof(PbrPushConstants), &pushConstants);

                    // Bind vertex and index buffers
                    vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.vertexBuffer_, offsets);
                    vkCmdBindIndexBuffer(cmd, mesh.indexBuffer_, 0, VK_INDEX_TYPE_UINT32);

                    // Draw the mesh
                    vkCmdDrawIndexed(cmd, static_cast<uint32_t>(mesh.indices_.size()), 1, 0, 0, 0);
                }
            }
        }

        vkCmdEndRendering(cmd);
    }
}

void Renderer::createPipelines(const VkFormat swapChainColorFormat, const VkFormat depthFormat,
                               VkSampleCountFlagBits msaaSamples)
{
    // Add render nodes in the order of execution

    // TEST render graph, don't delete the following comments
    // renderGraph_.addRenderNode({
    //     {"shadowMap"},
    //     {},          // no MSAA color attachments
    //     "",          // no MSAA depth attachments
    //     "",          // no MSAA stencil attachments
    //     {},          // no color attachments (depth only)
    //     "shadowMap", // depth attachment
    //     ""           // no stencil attachments
    // });
    // renderGraph_.addRenderNode({
    //     {"pbrForward", "sky"},
    //     {"msaaColor"},      // MSAA color attachment
    //     "msaaDepthStencil", // MSAA depth attachment
    //     "msaaDepthStencil", // MSAA stencil attachment
    //     {"floatColor1"},    // resolved color attachment
    //     "depthStencil",     // resolved depth attachment
    //     "depthStencil"      // resolved stencil attachment
    // });
    // renderGraph_.addRenderNode({
    //     {"ssao"},
    //     {}, // no MSAA color attachments (compute)
    //     "", // no MSAA depth attachments (compute)
    //     "", // no MSAA stencil attachments (compute)
    //     {}, // no color attachments (compute)
    //     "", // no depth attachments (compute)
    //     ""  // no stencil attachments (compute)
    // });
    // renderGraph_.addRenderNode({
    //     {"post"},
    //     {},            // no MSAA color attachments
    //     "",            // no MSAA depth attachments
    //     "",            // no MSAA stencil attachments
    //     {"swapchain"}, // color attachment (swapchain)
    //     "",            // no depth attachments
    //     ""             // no stencil attachments
    // });
    // renderGraph_.writeToFile("RenderGraph.json");
    renderGraph_.readFromFile("RenderGraph.json");

    pipelines_["pbrForward"] =
        make_unique<Pipeline>(ctx_, shaderManager_, PipelineConfig::createPbrForward(),
                              VK_FORMAT_R16G16B16A16_SFLOAT, depthFormat, msaaSamples);

    pipelines_["sky"] =
        make_unique<Pipeline>(ctx_, shaderManager_, PipelineConfig::createSky(),
                              VK_FORMAT_R16G16B16A16_SFLOAT, depthFormat, msaaSamples);

    pipelines_["post"] =
        make_unique<Pipeline>(ctx_, shaderManager_, PipelineConfig::createPost(),
                              swapChainColorFormat, depthFormat, VK_SAMPLE_COUNT_1_BIT);

    pipelines_["shadowMap"] =
        make_unique<Pipeline>(ctx_, shaderManager_, PipelineConfig::createShadowMap(),
                              VK_FORMAT_D16_UNORM, VK_FORMAT_D16_UNORM, VK_SAMPLE_COUNT_1_BIT);

    pipelines_["ssao"] =
        make_unique<Pipeline>(ctx_, shaderManager_, PipelineConfig::createSsao(),
                              VK_FORMAT_D16_UNORM, VK_FORMAT_D16_UNORM, VK_SAMPLE_COUNT_1_BIT);
}

void Renderer::createTextures(uint32_t swapchainWidth, uint32_t swapchainHeight,
                              VkSampleCountFlagBits msaaSamples)
{
    samplerLinearRepeat_.createLinearRepeat();
    samplerLinearClamp_.createLinearClamp();
    samplerAnisoRepeat_.createAnisoRepeat();
    samplerAnisoClamp_.createAnisoClamp();
    samplerShadow_.createShadow();

    // Initialize all image buffers in the consolidated map
    const vector<string> imageNames = {"msaaColor",      "msaaDepthStencil", "depthStencil",
                                       "floatColor1",    "floatColor2",      "shadowMap",
                                       "prefilteredMap", "irradianceMap",    "brdfLut"};

    for (const auto& name : imageNames) {
        imageBuffers_[name] = make_unique<Image2D>(ctx_);
    }

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
    imageBuffers_["irradianceMap"]->createTextureFromKtx2(path + "diffuseLambertian.ktx2", true);
    imageBuffers_["irradianceMap"]->setSampler(samplerLinearRepeat_.handle());

    // Load BRDF lookup table (2D texture)
    imageBuffers_["brdfLut"]->createTextureFromImage(path + "outputLUT.png", false, false);
    imageBuffers_["brdfLut"]->setSampler(samplerLinearClamp_.handle());

    // Create render targets with specified dimensions and formats
    imageBuffers_["msaaColor"]->createMsaaColorBuffer(swapchainWidth, swapchainHeight, msaaSamples);
    imageBuffers_["msaaDepthStencil"]->createDepthBuffer(swapchainWidth, swapchainHeight,
                                                         msaaSamples);
    imageBuffers_["depthStencil"]->createDepthBuffer(swapchainWidth, swapchainHeight,
                                                     VK_SAMPLE_COUNT_1_BIT);
    imageBuffers_["floatColor1"]->createGeneralStorage(swapchainWidth, swapchainHeight);
    imageBuffers_["floatColor2"]->createGeneralStorage(swapchainWidth, swapchainHeight);

    // Create shadow map
    uint32_t shadowMapSize = 2048 * 2;
    imageBuffers_["shadowMap"]->createShadow(shadowMapSize, shadowMapSize);
    imageBuffers_["shadowMap"]->setSampler(samplerShadow_.handle());

    // Set samplers for storage and sampling images
    imageBuffers_["floatColor1"]->setSampler(samplerLinearRepeat_.handle());
    imageBuffers_["floatColor2"]->setSampler(samplerLinearRepeat_.handle());
    imageBuffers_["depthStencil"]->setSampler(samplerLinearClamp_.handle());
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
                              const VkRenderingAttachmentInfo* depthAttachment,
                              const VkRenderingAttachmentInfo* stencilAttachment) const
{
    VkRenderingInfo renderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO_KHR};
    renderingInfo.renderArea = renderArea;
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = colorAttachment ? 1 : 0;
    renderingInfo.pColorAttachments = colorAttachment;
    renderingInfo.pDepthAttachment = depthAttachment;
    renderingInfo.pStencilAttachment = stencilAttachment;

    return renderingInfo;
}

} // namespace hlab