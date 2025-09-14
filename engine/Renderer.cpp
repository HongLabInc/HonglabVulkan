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

    unordered_map<string, vector<vector<BindingInfo>>> bindingInfos = shaderManager_.bindingInfos();

    for (auto& node : renderGraph_.renderNodes_) {

        cout << node.pipeline << endl;

        auto& bindings = bindingInfos.at(node.pipeline);

        assert(bindings.size() == node.descriptorSets.size());

        for (int s = 0; s < bindings.size(); s++) {

            string setName = node.descriptorSets[s];

            if (perFrameDescriptorSets_.find(setName) != perFrameDescriptorSets_.end())
                continue;
            if (descriptorSets_.find(setName) != descriptorSets_.end())
                continue;

            cout << "Set " << s << " " << node.descriptorSets[s] << endl;

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
                        ctx_, pipelines_[node.pipeline]->layouts()[s], resources);
                }
            } else {
                // Collect resources for non-per-frame descriptor set
                vector<reference_wrapper<Resource>> resources;

                for (const string& resourceName : bindingNames) {
                    addResource(resourceName, uint32_t(-1), resources);
                }

                // Create the descriptor set with collected resources
                descriptorSets_[setName].create(ctx_, pipelines_[node.pipeline]->layouts()[s],
                                                resources);
            }
        }
    }

    // Create descriptor sets using the consolidated map structures
    // perFrameDescriptorSets_["sceneOptions"].resize(kMaxFramesInFlight_);
    // for (size_t i = 0; i < kMaxFramesInFlight_; i++) {
    //    perFrameDescriptorSets_["sceneOptions"][i].create(
    //        ctx_, {*perFrameUniformBuffers_["sceneData"][i],
    //        *perFrameUniformBuffers_["options"][i],
    //               *perFrameUniformBuffers_["boneData"][i]});
    //}

    // perFrameDescriptorSets_["skyOptions"].resize(kMaxFramesInFlight_);
    // for (size_t i = 0; i < kMaxFramesInFlight_; i++) {
    //     perFrameDescriptorSets_["skyOptions"][i].create(
    //         ctx_,
    //         {*perFrameUniformBuffers_["sceneData"][i],
    //         *perFrameUniformBuffers_["skyOptions"][i]});
    // }

    // perFrameDescriptorSets_["postProcessing"].resize(kMaxFramesInFlight_);
    // for (size_t i = 0; i < kMaxFramesInFlight_; i++) {
    //     perFrameDescriptorSets_["postProcessing"][i].create(
    //         ctx_, {*imageBuffers_["floatColor2"], *perFrameUniformBuffers_["postOptions"][i]});
    // }

    // Create SSAO descriptor sets
    // Ensure floatColor1 and floatColor2 are properly configured for storage binding
    // floatColor1 will be used as readonly storage image (input)
    // floatColor2 will be used as writeonly storage image (output)
    // Create a command buffer for image layout transitions
    // auto cmd = ctx_.createGraphicsCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    // imageBuffers_["floatColor1"]->transitionToGeneral(cmd.handle(), VK_ACCESS_2_SHADER_READ_BIT,
    //                                                  VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
    // imageBuffers_["floatColor2"]->transitionToGeneral(cmd.handle(), VK_ACCESS_2_SHADER_WRITE_BIT,
    //                                                  VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
    // cmd.submitAndWait();
    // perFrameDescriptorSets_["ssao"].resize(kMaxFramesInFlight_);
    // for (size_t i = 0; i < kMaxFramesInFlight_; i++) {
    //    perFrameDescriptorSets_["ssao"][i].create(
    //        ctx_, {*perFrameUniformBuffers_["sceneData"][i],
    //               *perFrameUniformBuffers_["ssaoOptions"][i], *imageBuffers_["floatColor1"],
    //               *imageBuffers_["floatColor2"], *imageBuffers_["depthStencil"]});
    //}

    //// Create descriptor sets using consolidated image buffers and maps
    // descriptorSets_["sky"].create(ctx_,
    //                               {*imageBuffers_["prefilteredMap"],
    //                                *imageBuffers_["irradianceMap"], *imageBuffers_["brdfLut"]});
    // descriptorSets_["shadowMap"].create(ctx_, {*imageBuffers_["shadowMap"]});

    //// Create single descriptor set for all materials
    // descriptorSets_["material"].create(ctx_, {*materialBuffer_, *materialTextures_});
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
    VkRect2D renderArea = {0, 0, scissor.extent.width, scissor.extent.height};

    // Transition shadow map image to depth-stencil attachment layout
    imageBuffers_["shadowMap"]->transitionToDepthStencilAttachment(cmd);

    VkRenderingAttachmentInfo shadowDepthAttachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    shadowDepthAttachment.imageView = imageBuffers_["shadowMap"]->view();
    shadowDepthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    shadowDepthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    shadowDepthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    shadowDepthAttachment.clearValue.depthStencil = {1.0f, 0};

    VkRenderingInfo shadowRenderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO_KHR};
    shadowRenderingInfo.renderArea = {0, 0, imageBuffers_["shadowMap"]->width(),
                                      imageBuffers_["shadowMap"]->height()};
    shadowRenderingInfo.layerCount = 1;
    shadowRenderingInfo.colorAttachmentCount = 0;
    shadowRenderingInfo.pDepthAttachment = &shadowDepthAttachment;

    VkViewport shadowViewport{0.0f,
                              0.0f,
                              (float)imageBuffers_["shadowMap"]->width(),
                              (float)imageBuffers_["shadowMap"]->height(),
                              0.0f,
                              1.0f};
    VkRect2D shadowScissor{0, 0, imageBuffers_["shadowMap"]->width(),
                           imageBuffers_["shadowMap"]->height()};

    vkCmdBeginRendering(cmd, &shadowRenderingInfo);
    vkCmdSetViewport(cmd, 0, 1, &shadowViewport);
    vkCmdSetScissor(cmd, 0, 1, &shadowScissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines_.at("shadowMap")->pipeline());

    const auto descriptorSets =
        vector{perFrameDescriptorSets_["sceneOptions"][currentFrame].handle()};

    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines_.at("shadowMap")->pipelineLayout(), 0,
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

        vkCmdPushConstants(cmd, pipelines_.at("shadowMap")->pipelineLayout(),
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

    // Forward rendering pass - Batch all initial transitions
    {
        // Transition shadow map to shader read-only for sampling in main render pass
        imageBuffers_["shadowMap"]->transitionToShaderRead(cmd);

        VkImageMemoryBarrier2 imageBarriers[2];

        // floatColor1: UNDEFINED → COLOR_ATTACHMENT for MSAA resolve target
        imageBarriers[0] = imageBuffers_["floatColor1"]->barrierHelper().prepareBarrier(
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

        // msaaColor: UNDEFINED → COLOR_ATTACHMENT for MSAA rendering
        imageBarriers[1] = imageBuffers_["msaaColor"]->barrierHelper().prepareBarrier(
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

        VkDependencyInfo depInfo{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        depInfo.imageMemoryBarrierCount = 2;
        depInfo.pImageMemoryBarriers = imageBarriers;

        vkCmdPipelineBarrier2(cmd, &depInfo);

        auto colorAttachment =
            createColorAttachment(imageBuffers_["msaaColor"]->view(), VK_ATTACHMENT_LOAD_OP_CLEAR,
                                  {0.0f, 0.0f, 0.5f, 0.0f}, imageBuffers_["floatColor1"]->view(),
                                  VK_RESOLVE_MODE_AVERAGE_BIT);
        auto depthAttachment = createDepthAttachment(
            imageBuffers_["msaaDepthStencil"]->attachmentView(), VK_ATTACHMENT_LOAD_OP_CLEAR, 1.0f,
            imageBuffers_["depthStencil"]->attachmentView(), VK_RESOLVE_MODE_SAMPLE_ZERO_BIT);
        auto renderingInfo = createRenderingInfo(renderArea, &colorAttachment, &depthAttachment);

        vkCmdBeginRendering(cmd, &renderingInfo);
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        VkDeviceSize offsets[1]{0};

        // Render models
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipelines_.at("pbrForward")->pipeline());

        // Bind descriptor sets once per model (no longer per material)
        const auto descriptorSets = vector{
            perFrameDescriptorSets_["sceneOptions"][currentFrame]
                .handle(),                        // Set 0: scene, options, bone data
            descriptorSets_["material"].handle(), // Set 1: material storage buffer + textures
            descriptorSets_["sky"].handle(),      // Set 2: sky textures
            descriptorSets_["shadowMap"].handle() // Set 3: shadow map
        };

        vkCmdBindDescriptorSets(
            cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines_.at("pbrForward")->pipelineLayout(), 0,
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
                vkCmdPushConstants(cmd, pipelines_.at("pbrForward")->pipelineLayout(),
                                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                   sizeof(PbrPushConstants), &pushConstants);

                vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.vertexBuffer_, offsets);
                vkCmdBindIndexBuffer(cmd, mesh.indexBuffer_, 0, VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(cmd, static_cast<uint32_t>(mesh.indices_.size()), 1, 0, 0, 0);
            }
        }

        // Sky rendering pass
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines_.at("sky")->pipeline());

        const auto skyDescriptorSets = vector{
            perFrameDescriptorSets_["skyOptions"][currentFrame]
                .handle(),                  // Set 0: scene + sky options
            descriptorSets_["sky"].handle() // Set 1: sky textures
        };

        vkCmdBindDescriptorSets(
            cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines_.at("sky")->pipelineLayout(), 0,
            static_cast<uint32_t>(skyDescriptorSets.size()), skyDescriptorSets.data(), 0, nullptr);
        vkCmdDraw(cmd, 36, 1, 0, 0);
        vkCmdEndRendering(cmd);
    }

    // Graphics to Compute transition - Batch all transitions for SSAO
    {
        VkMemoryBarrier2 memoryBarrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
        memoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        memoryBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        memoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;

        VkImageMemoryBarrier2 imageBarriers[3];

        // floatColor1: COLOR_ATTACHMENT → GENERAL (SSAO input)
        imageBarriers[0] = imageBuffers_["floatColor1"]->barrierHelper().prepareBarrier(
            VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_2_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

        // floatColor2: UNDEFINED → GENERAL (SSAO output)
        imageBarriers[1] = imageBuffers_["floatColor2"]->barrierHelper().prepareBarrier(
            VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_2_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

        // depthStencil: DEPTH_STENCIL_ATTACHMENT → SHADER_READ_ONLY (for depth sampling)
        imageBarriers[2] = imageBuffers_["depthStencil"]->barrierHelper().prepareBarrier(
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_2_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

        VkDependencyInfo depInfo{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        depInfo.memoryBarrierCount = 1;
        depInfo.pMemoryBarriers = &memoryBarrier;
        depInfo.imageMemoryBarrierCount = 3;
        depInfo.pImageMemoryBarriers = imageBarriers;

        vkCmdPipelineBarrier2(cmd, &depInfo);

        // Bind SSAO compute pipeline
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines_.at("ssao")->pipeline());

        // Bind descriptor sets for SSAO
        const auto ssaoDescriptorSets =
            vector{perFrameDescriptorSets_["ssao"][currentFrame].handle()};
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                pipelines_.at("ssao")->pipelineLayout(), 0,
                                static_cast<uint32_t>(ssaoDescriptorSets.size()),
                                ssaoDescriptorSets.data(), 0, nullptr);

        // Dispatch compute shader
        // Calculate dispatch size based on image dimensions and local work group size (16x16)
        uint32_t groupCountX = (scissor.extent.width + 15) / 16;  // Round up division
        uint32_t groupCountY = (scissor.extent.height + 15) / 16; // Round up division
        vkCmdDispatch(cmd, groupCountX, groupCountY, 1);
    }

    // Compute to Graphics transition for Post-processing
    {
        VkImageMemoryBarrier2 imageBarrier =
            imageBuffers_["floatColor2"]->barrierHelper().prepareBarrier(
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_2_SHADER_READ_BIT,
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);

        VkDependencyInfo depInfo{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers = &imageBarrier;

        vkCmdPipelineBarrier2(cmd, &depInfo);

        // Post-processing pass
        auto colorAttachment = createColorAttachment(
            swapchainImageView, VK_ATTACHMENT_LOAD_OP_CLEAR, {0.0f, 0.0f, 1.0f, 0.0f});

        // No depth attachment needed for post-processing
        auto renderingInfo = createRenderingInfo(renderArea, &colorAttachment, nullptr);

        vkCmdBeginRendering(cmd, &renderingInfo);
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines_.at("post")->pipeline());

        const auto postDescriptorSets =
            vector{perFrameDescriptorSets_["postProcessing"][currentFrame].handle()};
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipelines_.at("post")->pipelineLayout(), 0,
                                static_cast<uint32_t>(postDescriptorSets.size()),
                                postDescriptorSets.data(), 0, nullptr);

        vkCmdDraw(cmd, 6, 1, 0, 0);
        vkCmdEndRendering(cmd);
    }
}

void Renderer::createPipelines(const VkFormat swapChainColorFormat, const VkFormat depthFormat,
                               VkSampleCountFlagBits msaaSamples)
{
    // Add render nodes in the order of execution

    // 1. Shadow Map Pass - Generate shadow maps first
    renderGraph_.addRenderNode({"shadowMap",
                                {},            // no MSAA color attachments
                                {},            // no MSAA depth attachments
                                {},            // no MSAA stencil attachments
                                {},            // no color attachments (depth only)
                                {"shadowMap"}, // depth attachment
                                {},            // no stencil attachments
                                {"sceneOptions"}});

    // 2. PBR Forward Pass - Main geometry rendering with MSAA resolve
    renderGraph_.addRenderNode({"pbrForward",
                                {"msaaColor"},        // MSAA color attachment
                                {"msaaDepthStencil"}, // MSAA depth attachment
                                {"msaaDepthStencil"}, // MSAA stencil attachment
                                {"floatColor1"},      // resolved color attachment
                                {"depthStencil"},     // resolved depth attachment
                                {"depthStencil"},     // resolved stencil attachment
                                {"sceneOptions", "material", "sky", "shadowMap"}});

    // 3. Sky Pass - Rendered within the same render pass but separate pipeline
    renderGraph_.addRenderNode({"sky",
                                {"msaaColor"},        // same MSAA color attachment as pbrForward
                                {"msaaDepthStencil"}, // same MSAA depth attachment
                                {"msaaDepthStencil"}, // same MSAA stencil attachment
                                {"floatColor1"},      // same resolved color attachment
                                {"depthStencil"},     // same resolved depth attachment
                                {"depthStencil"},     // same resolved stencil attachment
                                {"skyOptions", "sky"}});

    // 4. SSAO Compute Pass - Screen Space Ambient Occlusion
    renderGraph_.addRenderNode({"ssao",
                                {}, // no MSAA color attachments (compute)
                                {}, // no MSAA depth attachments (compute)
                                {}, // no MSAA stencil attachments (compute)
                                {}, // no color attachments (compute)
                                {}, // no depth attachments (compute)
                                {}, // no stencil attachments (compute)
                                {"ssao"}});

    // 5. Post-Processing Pass - Final output to swapchain
    renderGraph_.addRenderNode({"post",
                                {},            // no MSAA color attachments
                                {},            // no MSAA depth attachments
                                {},            // no MSAA stencil attachments
                                {"swapchain"}, // color attachment (swapchain)
                                {},            // no depth attachments
                                {},            // no stencil attachments
                                {"postProcessing"}});

    // Create pipelines using unique_ptr and PipelineConfig-based creation
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