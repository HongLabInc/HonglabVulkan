#include "DescriptorSet.h"
#include "Pipeline.h"
#include "Vertex.h"
#include "Image2D.h"
#include <imgui.h>

namespace hlab {

void Pipeline::cleanup()
{
    if (pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(ctx_.device(), pipeline_, nullptr);
        pipeline_ = VK_NULL_HANDLE;
    }
    if (pipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(ctx_.device(), pipelineLayout_, nullptr);
        pipelineLayout_ = VK_NULL_HANDLE;
    }

    // Do not cleanup descriptorSetLayouts here
}

void Pipeline::createFromConfig(const PipelineConfig& config, vector<VkFormat> outColorFormats,
                                optional<VkFormat> depthFormat,
                                optional<VkSampleCountFlagBits> msaaSamples)
{
    name_ = config.name;

    // Set bindPoint_ based on pipeline type
    bindPoint_ = (config.type == PipelineConfig::Type::Compute) ? VK_PIPELINE_BIND_POINT_COMPUTE
                                                                : VK_PIPELINE_BIND_POINT_GRAPHICS;

    // Copy binding information from ShaderManager for this pipeline
    const auto& shaderManagerBindingInfos = shaderManager_.bindingInfos();
    auto it = shaderManagerBindingInfos.find(name_);
    if (it != shaderManagerBindingInfos.end()) {
        bindingInfos_ = it->second;
    }

    // Validate required formats
    validateRequiredFormats(config, outColorFormats, depthFormat, msaaSamples);

    createCommon();

    if (config.type == PipelineConfig::Type::Compute) {
        createCompute();
        // Initialize compute shader local workgroup size after pipeline creation
        initializeComputeLocalWorkgroupSize();
    } else {
        createGraphicsFromConfig(config, outColorFormats, depthFormat, msaaSamples);
    }
}

void Pipeline::validateRequiredFormats(const PipelineConfig& config,
                                       vector<VkFormat> outColorFormats,
                                       optional<VkFormat> depthFormat,
                                       optional<VkSampleCountFlagBits> msaaSamples)
{
    if (config.requiredFormats.outColorFormat && outColorFormats.empty()) {
        exitWithMessage("outColorFormats required for pipeline '{}'", config.name);
    }
    if (config.requiredFormats.depthFormat && !depthFormat.has_value()) {
        exitWithMessage("depthFormat required for pipeline '{}'", config.name);
    }
    if (config.requiredFormats.msaaSamples && !msaaSamples.has_value()) {
        exitWithMessage("msaaSamples required for pipeline '{}'", config.name);
    }
}

void Pipeline::createGraphicsFromConfig(const PipelineConfig& config,
                                        vector<VkFormat> outColorFormats,
                                        optional<VkFormat> depthFormat,
                                        optional<VkSampleCountFlagBits> msaaSamples)
{
    // Unified graphics pipeline creation using PipelineConfig
    const VkDevice device = ctx_.device();

    printLog("Creating graphics pipeline from config: {}", config.name);

    // Get shader stages
    vector<VkPipelineShaderStageCreateInfo> shaderStagesCI =
        shaderManager_.createPipelineShaderStageCIs(config.name);

    // ========================================================================
    // 1. VERTEX INPUT STATE
    // ========================================================================
    vector<VkVertexInputBindingDescription> vertexInputBindings;
    vector<VkVertexInputAttributeDescription> vertexInputAttributes;

    if (config.vertexInput.type == PipelineConfig::VertexInput::Type::Standard) {
        // Standard 3D vertex input (PBR Forward, Shadow Map)
        vertexInputBindings.resize(1);
        vertexInputBindings[0].binding = 0;
        vertexInputBindings[0].stride = sizeof(Vertex);
        vertexInputBindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        vertexInputAttributes = Vertex::getAttributeDescriptions();
    } else if (config.vertexInput.type == PipelineConfig::VertexInput::Type::ImGui) {
        // ImGui vertex input (GUI)
        vertexInputBindings.resize(1);
        vertexInputBindings[0].binding = 0;
        vertexInputBindings[0].stride = sizeof(ImDrawVert);
        vertexInputBindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        vertexInputAttributes.resize(3);
        vertexInputAttributes[0] = {0, 0, VK_FORMAT_R32G32_SFLOAT, 0};   // Position
        vertexInputAttributes[1] = {1, 0, VK_FORMAT_R32G32_SFLOAT, 8};   // UV
        vertexInputAttributes[2] = {2, 0, VK_FORMAT_R8G8B8A8_UNORM, 16}; // Color
    }
    // For Type::None, keep vectors empty (no vertex input)

    VkPipelineVertexInputStateCreateInfo vertexInputStateCI{};
    vertexInputStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputStateCI.vertexBindingDescriptionCount =
        static_cast<uint32_t>(vertexInputBindings.size());
    vertexInputStateCI.pVertexBindingDescriptions = vertexInputBindings.data();
    vertexInputStateCI.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(vertexInputAttributes.size());
    vertexInputStateCI.pVertexAttributeDescriptions = vertexInputAttributes.data();

    // ========================================================================
    // 2. INPUT ASSEMBLY STATE
    // ========================================================================
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI{};
    inputAssemblyStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyStateCI.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyStateCI.primitiveRestartEnable = VK_FALSE;

    // ========================================================================
    // 3. RASTERIZATION STATE
    // ========================================================================
    VkPipelineRasterizationStateCreateInfo rasterStateCI{};
    rasterStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterStateCI.depthClampEnable = config.rasterization.depthClampEnable ? VK_TRUE : VK_FALSE;
    rasterStateCI.rasterizerDiscardEnable = VK_FALSE;
    rasterStateCI.polygonMode = VK_POLYGON_MODE_FILL;
    rasterStateCI.cullMode = config.rasterization.cullMode;
    rasterStateCI.frontFace = config.rasterization.frontFace;
    rasterStateCI.depthBiasEnable = config.rasterization.depthBiasEnable ? VK_TRUE : VK_FALSE;
    rasterStateCI.depthBiasConstantFactor = config.rasterization.depthBiasConstantFactor;
    rasterStateCI.depthBiasClamp = 0.0f;
    rasterStateCI.depthBiasSlopeFactor = config.rasterization.depthBiasSlopeFactor;
    rasterStateCI.lineWidth = 1.0f;

    // ========================================================================
    // 4. COLOR BLEND STATE
    // ========================================================================
    // Create blend attachment states for each color attachment
    vector<VkPipelineColorBlendAttachmentState> blendAttachmentStates;

    if (!config.specialConfig.isDepthOnly && !outColorFormats.empty()) {
        blendAttachmentStates.resize(outColorFormats.size());

        for (size_t i = 0; i < outColorFormats.size(); ++i) {
            auto& blendAttachmentState = blendAttachmentStates[i];
            blendAttachmentState.blendEnable = config.colorBlend.blendEnable ? VK_TRUE : VK_FALSE;
            blendAttachmentState.colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                VK_COLOR_COMPONENT_A_BIT;

            if (config.colorBlend.blendEnable) {
                // Use alpha blending configuration
                blendAttachmentState.srcColorBlendFactor =
                    config.colorBlend.alphaBlending.srcColorBlendFactor;
                blendAttachmentState.dstColorBlendFactor =
                    config.colorBlend.alphaBlending.dstColorBlendFactor;
                blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
                blendAttachmentState.srcAlphaBlendFactor =
                    config.colorBlend.alphaBlending.srcAlphaBlendFactor;
                blendAttachmentState.dstAlphaBlendFactor =
                    config.colorBlend.alphaBlending.dstAlphaBlendFactor;
                blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
            } else {
                // No blending
                blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
                blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
                blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
                blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
                blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
            }
        }
    }

    VkPipelineColorBlendStateCreateInfo colorBlendStateCI{};
    colorBlendStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendStateCI.logicOpEnable = VK_FALSE;
    colorBlendStateCI.logicOp = VK_LOGIC_OP_COPY;
    colorBlendStateCI.attachmentCount = static_cast<uint32_t>(blendAttachmentStates.size());
    colorBlendStateCI.pAttachments =
        blendAttachmentStates.empty() ? nullptr : blendAttachmentStates.data();
    colorBlendStateCI.blendConstants[0] = 0.0f;
    colorBlendStateCI.blendConstants[1] = 0.0f;
    colorBlendStateCI.blendConstants[2] = 0.0f;
    colorBlendStateCI.blendConstants[3] = 0.0f;

    // ========================================================================
    // 5. DEPTH STENCIL STATE
    // ========================================================================
    VkPipelineDepthStencilStateCreateInfo depthStencilStateCI{};
    depthStencilStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilStateCI.depthTestEnable = config.depthStencil.depthTest ? VK_TRUE : VK_FALSE;
    depthStencilStateCI.depthWriteEnable = config.depthStencil.depthWrite ? VK_TRUE : VK_FALSE;
    depthStencilStateCI.depthCompareOp = config.depthStencil.depthCompareOp;
    depthStencilStateCI.depthBoundsTestEnable = VK_FALSE;
    depthStencilStateCI.stencilTestEnable = VK_FALSE;
    depthStencilStateCI.front.failOp = VK_STENCIL_OP_KEEP;
    depthStencilStateCI.front.passOp = VK_STENCIL_OP_KEEP;
    depthStencilStateCI.front.depthFailOp = VK_STENCIL_OP_KEEP;
    depthStencilStateCI.front.compareOp = VK_COMPARE_OP_ALWAYS;
    depthStencilStateCI.front.compareMask = 0;
    depthStencilStateCI.front.writeMask = 0;
    depthStencilStateCI.front.reference = 0;
    depthStencilStateCI.back = depthStencilStateCI.front;
    depthStencilStateCI.minDepthBounds = 0.0f;
    depthStencilStateCI.maxDepthBounds = 1.0f;

    // ========================================================================
    // 6. VIEWPORT STATE
    // ========================================================================
    VkPipelineViewportStateCreateInfo viewportStateCI{};
    viewportStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateCI.viewportCount = 1;
    viewportStateCI.pViewports = nullptr; // Dynamic
    viewportStateCI.scissorCount = 1;
    viewportStateCI.pScissors = nullptr; // Dynamic

    // ========================================================================
    // 7. DYNAMIC STATE
    // ========================================================================
    VkPipelineDynamicStateCreateInfo dynamicStateCI{};
    dynamicStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateCI.dynamicStateCount = static_cast<uint32_t>(config.dynamicState.states.size());
    dynamicStateCI.pDynamicStates = config.dynamicState.states.data();

    // ========================================================================
    // 8. MULTISAMPLE STATE
    // ========================================================================
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
    if (config.multisample.type == PipelineConfig::Multisample::Type::Variable &&
        msaaSamples.has_value()) {
        sampleCount = msaaSamples.value();
    }

    VkPipelineMultisampleStateCreateInfo multisampleStateCI{};
    multisampleStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleStateCI.rasterizationSamples = sampleCount;
    multisampleStateCI.sampleShadingEnable = VK_FALSE;
    multisampleStateCI.minSampleShading = 1.0f;
    multisampleStateCI.pSampleMask = nullptr;
    multisampleStateCI.alphaToCoverageEnable = VK_FALSE;
    multisampleStateCI.alphaToOneEnable = VK_FALSE;

    // ========================================================================
    // 9. PIPELINE RENDERING CREATE INFO (Vulkan 1.3 Dynamic Rendering)
    // ========================================================================
    VkPipelineRenderingCreateInfo pipelineRenderingCI{};
    pipelineRenderingCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    pipelineRenderingCI.viewMask = 0;

    // For depth-only pipelines (shadow maps), don't set any color attachments
    if (config.specialConfig.isDepthOnly) {
        pipelineRenderingCI.colorAttachmentCount = 0;
        pipelineRenderingCI.pColorAttachmentFormats = nullptr;
    } else {
        pipelineRenderingCI.colorAttachmentCount = static_cast<uint32_t>(outColorFormats.size());
        pipelineRenderingCI.pColorAttachmentFormats =
            outColorFormats.empty() ? nullptr : outColorFormats.data();
    }

    // Set depth format
    if (depthFormat.has_value()) {
        pipelineRenderingCI.depthAttachmentFormat = depthFormat.value();

        // Only set stencil format if the depth format has a stencil aspect
        // Common depth-stencil formats: VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT
        // Depth-only formats: VK_FORMAT_D16_UNORM, VK_FORMAT_D32_SFLOAT
        switch (depthFormat.value()) {
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
        case VK_FORMAT_D16_UNORM_S8_UINT:
            // These formats have stencil component
            pipelineRenderingCI.stencilAttachmentFormat = depthFormat.value();
            break;
        default:
            // Depth-only formats don't have stencil
            pipelineRenderingCI.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;
            break;
        }
    } else {
        pipelineRenderingCI.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
        pipelineRenderingCI.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;
    }

    // ========================================================================
    // 10. GRAPHICS PIPELINE CREATE INFO
    // ========================================================================
    VkGraphicsPipelineCreateInfo pipelineCI{};
    pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCI.pNext = &pipelineRenderingCI;
    pipelineCI.flags = 0;
    pipelineCI.stageCount = static_cast<uint32_t>(shaderStagesCI.size());
    pipelineCI.pStages = shaderStagesCI.data();
    pipelineCI.pVertexInputState = &vertexInputStateCI;
    pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
    pipelineCI.pTessellationState = nullptr;
    pipelineCI.pViewportState = &viewportStateCI;
    pipelineCI.pRasterizationState = &rasterStateCI;
    pipelineCI.pMultisampleState = &multisampleStateCI;
    pipelineCI.pDepthStencilState = &depthStencilStateCI;
    pipelineCI.pColorBlendState = &colorBlendStateCI;
    pipelineCI.pDynamicState = &dynamicStateCI;
    pipelineCI.layout = pipelineLayout_;
    pipelineCI.renderPass = VK_NULL_HANDLE; // Using dynamic rendering
    pipelineCI.subpass = 0;
    pipelineCI.basePipelineHandle = VK_NULL_HANDLE;
    pipelineCI.basePipelineIndex = -1;

    // Create the graphics pipeline
    check(vkCreateGraphicsPipelines(device, ctx_.pipelineCache(), 1, &pipelineCI, nullptr,
                                    &pipeline_));

    printLog("Successfully created graphics pipeline: {}", config.name);
}

VkPipeline Pipeline::pipeline() const
{
    return pipeline_;
}

VkPipelineLayout Pipeline::pipelineLayout() const
{
    return pipelineLayout_;
}

ShaderManager& Pipeline::shaderManager()
{
    return shaderManager_;
}

void Pipeline::setDescriptorSets(
    vector<vector<reference_wrapper<DescriptorSet>>>& descriptorSets) // <- use this instead
{
    descriptorSets_ = descriptorSets;

    // Update descriptorSetHandles_ to match the same structure as descriptorSets_
    descriptorSetHandles_.clear();
    descriptorSetHandles_.resize(descriptorSets_.size());

    for (size_t frameIndex = 0; frameIndex < descriptorSets_.size(); ++frameIndex) {
        descriptorSetHandles_[frameIndex].reserve(descriptorSets_[frameIndex].size());

        for (const auto& descriptorSetRef : descriptorSets_[frameIndex]) {
            descriptorSetHandles_[frameIndex].push_back(descriptorSetRef.get().handle());
        }
    }

    // Determine dimensions from first write-only binding if not already set
    if (width_ == 0 && height_ == 0) {
        determineDimensionsFromFirstWriteOnlyBinding();
    }
}

void Pipeline::determineDimensionsFromFirstWriteOnlyBinding()
{
    // Search through all descriptor sets and bindings for the first write-only binding
    for (size_t setIndex = 0; setIndex < bindingInfos_.size(); ++setIndex) {
        for (size_t bindingIndex = 0; bindingIndex < bindingInfos_[setIndex].size();
             ++bindingIndex) {
            const auto& bindingInfo = bindingInfos_[setIndex][bindingIndex];

            if (bindingInfo.writeonly && !bindingInfo.resourceName.empty()) {
                // Found first write-only binding, now get its dimensions
                assert(!descriptorSets_.empty() &&
                       "No descriptor sets available for dimension determination");
                assert(setIndex < descriptorSets_[0].size() && "Set index out of bounds");

                const auto& descriptorSet = descriptorSets_[0][setIndex].get();
                const auto& resources = descriptorSet.resources();

                assert(bindingIndex < resources.size() && "Binding index out of bounds");

                const Resource& resource = resources[bindingIndex].get();

                // Check if this is an Image resource and get its dimensions
                if (resource.isImage()) {
                    const Image2D* image = dynamic_cast<const Image2D*>(&resource);
                    if (image) {
                        width_ = image->width();
                        height_ = image->height();

                        printLog("Pipeline '{}' dimensions determined from first write-only "
                                 "binding '{}': {}x{}",
                                 name_, bindingInfo.resourceName, width_, height_);
                        return;
                    }
                }
            }
        }
    }

    // If no write-only image binding found, log a warning
    if (width_ == 0 && height_ == 0) {
        printLog("Pipeline '{}': No write-only image binding found for dimension determination",
                 name_);
    }
}

void Pipeline::initializeComputeLocalWorkgroupSize()
{
    // Only initialize for compute pipelines
    if (bindPoint_ != VK_PIPELINE_BIND_POINT_COMPUTE) {
        return;
    }

    // Get local workgroup size from shader manager
    auto localSize = shaderManager_.getComputeLocalWorkgroupSize(name_);
    local_size_ = localSize;

    printLog("Pipeline '{}' initialized with local workgroup size: {}x{}x{}", name_, local_size_[0],
             local_size_[1], local_size_[2]);
}

void Pipeline::submitBarriers(const VkCommandBuffer& cmd, uint32_t frameIndex)
{
    // Use asserts for boundary checks instead of cascaded if-clauses
    assert(frameIndex < descriptorSets_.size() && "Frame index out of bounds");

    // Iterate through all descriptor sets for this frame
    for (size_t setIndex = 0; setIndex < descriptorSets_[frameIndex].size(); ++setIndex) {
        // Get the descriptor set and corresponding binding info
        const auto& descriptorSet = descriptorSets_[frameIndex][setIndex].get();

        // Assert that we have binding info for this set
        assert(setIndex < bindingInfos_.size() && "Set index out of bounds in bindingInfos_");

        const auto& setBindingInfos = bindingInfos_[setIndex];
        const auto& resources = descriptorSet.resources();

        // Iterate through all bindings in this set
        for (size_t bindingIndex = 0;
             bindingIndex < resources.size() && bindingIndex < setBindingInfos.size();
             ++bindingIndex) {

            const auto& resource = resources[bindingIndex].get();
            const auto& bindingInfo = setBindingInfos[bindingIndex];

            // Only transition image resources (buffers don't need layout transitions)
            if (resource.isImage() && bindingInfo.targetLayout != VK_IMAGE_LAYOUT_UNDEFINED) {
                // Cast to non-const to allow transition (this is safe as we're modifying state)
                auto& mutableResource = const_cast<Resource&>(resource);

                // Transition the image to the desired layout for this binding
                mutableResource.transitionTo(cmd, bindingInfo.targetAccess,
                                             bindingInfo.targetLayout, bindingInfo.targetStage);
            }
        }
    }
}

void Pipeline::createCommon()
{
    cleanup();

    layouts_ = ctx_.descriptorPool().layoutsForPipeline(name_);

    VkPushConstantRange pushConstantRanges = shaderManager_.pushConstantsRange(name_);

    VkPipelineLayoutCreateInfo pipelineLayoutCI{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineLayoutCI.setLayoutCount = uint32_t(layouts_.size());
    pipelineLayoutCI.pSetLayouts = layouts_.data();
    pipelineLayoutCI.pushConstantRangeCount = (pushConstantRanges.size > 0) ? 1 : 0;
    pipelineLayoutCI.pPushConstantRanges =
        (pushConstantRanges.size > 0) ? &pushConstantRanges : nullptr;
    check(vkCreatePipelineLayout(ctx_.device(), &pipelineLayoutCI, nullptr, &pipelineLayout_));

    // printLog("pipelineLayout 0x{:x}", reinterpret_cast<uintptr_t>(pipelineLayout_));
}

} // namespace hlab