#include "Pipeline.h"
#include "Vertex.h"
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

void Pipeline::createFromConfig(const PipelineConfig& config, optional<VkFormat> outColorFormat,
                                optional<VkFormat> depthFormat,
                                optional<VkSampleCountFlagBits> msaaSamples)
{
    name_ = config.name;

    // Validate required formats
    validateRequiredFormats(config, outColorFormat, depthFormat, msaaSamples);

    createCommon();

    if (config.type == PipelineConfig::Type::Compute) {
        createCompute();
    } else {
        createGraphicsFromConfig(config, outColorFormat.value(), depthFormat, msaaSamples);
    }
}

void Pipeline::validateRequiredFormats(const PipelineConfig& config,
                                       optional<VkFormat> outColorFormat,
                                       optional<VkFormat> depthFormat,
                                       optional<VkSampleCountFlagBits> msaaSamples)
{
    if (config.requiredFormats.outColorFormat && !outColorFormat.has_value()) {
        exitWithMessage("outColorFormat required for pipeline '{}'", config.name);
    }
    if (config.requiredFormats.depthFormat && !depthFormat.has_value()) {
        exitWithMessage("depthFormat required for pipeline '{}'", config.name);
    }
    if (config.requiredFormats.msaaSamples && !msaaSamples.has_value()) {
        exitWithMessage("msaaSamples required for pipeline '{}'", config.name);
    }
}

void Pipeline::createGraphicsFromConfig(const PipelineConfig& config, VkFormat outColorFormat,
                                        optional<VkFormat> depthFormat,
                                        optional<VkSampleCountFlagBits> msaaSamples)
{
    // Unified graphics pipeline creation using PipelineConfig
    const VkDevice device = ctx_.device();

    printLog("Creating graphics pipeline from config: {}", config.name);

    // Get shader stages
    vector<VkPipelineShaderStageCreateInfo> shaderStagesCI =
        shaderManager_.createPipelineShaderStageCIs(config.name);

    // Prepare color formats
    vector<VkFormat> outColorFormats;
    if (!config.specialConfig.isDepthOnly) {
        outColorFormats.push_back(outColorFormat);
    }

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
    VkPipelineColorBlendAttachmentState blendAttachmentState{};
    blendAttachmentState.blendEnable = config.colorBlend.blendEnable ? VK_TRUE : VK_FALSE;
    blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

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

    VkPipelineColorBlendStateCreateInfo colorBlendStateCI{};
    colorBlendStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendStateCI.logicOpEnable = VK_FALSE;
    colorBlendStateCI.logicOp = VK_LOGIC_OP_COPY;
    colorBlendStateCI.attachmentCount = config.specialConfig.isDepthOnly ? 0 : 1;
    colorBlendStateCI.pAttachments =
        config.specialConfig.isDepthOnly ? nullptr : &blendAttachmentState;
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
    pipelineRenderingCI.colorAttachmentCount = static_cast<uint32_t>(outColorFormats.size());
    pipelineRenderingCI.pColorAttachmentFormats =
        outColorFormats.empty() ? nullptr : outColorFormats.data();

    // Set depth format
    if (config.specialConfig.isDepthOnly) {
        pipelineRenderingCI.depthAttachmentFormat =
            outColorFormat; // For shadow maps, outColorFormat is actually depth format
        pipelineRenderingCI.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;
    } else if (depthFormat.has_value()) {
        pipelineRenderingCI.depthAttachmentFormat = depthFormat.value();
        pipelineRenderingCI.stencilAttachmentFormat = depthFormat.value();
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

void Pipeline::createCommon()
{
    cleanup();

    vector<VkDescriptorSetLayout> layouts = ctx_.descriptorPool().layoutsForPipeline(name_);
    VkPushConstantRange pushConstantRanges = shaderManager_.pushConstantsRange(name_);

    VkPipelineLayoutCreateInfo pipelineLayoutCI{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineLayoutCI.setLayoutCount = uint32_t(layouts.size());
    pipelineLayoutCI.pSetLayouts = layouts.data();
    pipelineLayoutCI.pushConstantRangeCount = (pushConstantRanges.size > 0) ? 1 : 0;
    pipelineLayoutCI.pPushConstantRanges =
        (pushConstantRanges.size > 0) ? &pushConstantRanges : nullptr;
    check(vkCreatePipelineLayout(ctx_.device(), &pipelineLayoutCI, nullptr, &pipelineLayout_));

    // printLog("pipelineLayout 0x{:x}", reinterpret_cast<uintptr_t>(pipelineLayout_));
}

} // namespace hlab