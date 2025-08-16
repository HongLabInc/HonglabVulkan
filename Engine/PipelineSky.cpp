#include "Pipeline.h"

namespace hlab {

void Pipeline::createSky(VkFormat outColorFormat, VkFormat depthFormat,
                         VkSampleCountFlagBits msaaSamples)
{
    const uint32_t sizeofVertexStructure = 0; // No vertex input
    const VkDevice device = ctx_.device();

    printLog("Creating a graphics pipeline: {}\n", name_);

    // pipeline_ 생성

    vector<VkVertexInputAttributeDescription> vertexInputAttributes{};

    vector<VkPipelineShaderStageCreateInfo> shaderStagesCI =
        shaderManager_.createPipelineShaderStageCIs(name_);

    vector<VkFormat> outColorFormats{outColorFormat};

    // No vertex input for skybox (geometry is hardcoded in vertex shader)
    // vector<VkVertexInputBindingDescription> vertexInputBindingDesc;
    // vertexInputBindingDesc.resize(1); // Assuming one binding for simplicity
    // vertexInputBindingDesc[0].binding = 0;
    // vertexInputBindingDesc[0].stride = sizeofVertexStructure;
    // vertexInputBindingDesc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexInputStateCI;
    vertexInputStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputStateCI.pNext = nullptr;
    vertexInputStateCI.flags = 0;
    vertexInputStateCI.vertexBindingDescriptionCount = 0;
    vertexInputStateCI.pVertexBindingDescriptions = nullptr;
    vertexInputStateCI.vertexAttributeDescriptionCount = 0;
    vertexInputStateCI.pVertexAttributeDescriptions = nullptr;

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI;
    inputAssemblyStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyStateCI.pNext = nullptr;
    inputAssemblyStateCI.flags = 0;
    inputAssemblyStateCI.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyStateCI.primitiveRestartEnable = VK_FALSE;

    VkPipelineRasterizationStateCreateInfo rasterStateCI;
    rasterStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterStateCI.pNext = nullptr;
    rasterStateCI.flags = 0;
    rasterStateCI.depthClampEnable = VK_FALSE;
    rasterStateCI.rasterizerDiscardEnable = VK_FALSE;
    rasterStateCI.polygonMode = VK_POLYGON_MODE_FILL;
    rasterStateCI.cullMode = VK_CULL_MODE_NONE;
    rasterStateCI.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterStateCI.depthBiasEnable = VK_FALSE;
    rasterStateCI.depthBiasConstantFactor = 0.0f;
    rasterStateCI.depthBiasClamp = 0.0f;
    rasterStateCI.depthBiasSlopeFactor = 0.0f;
    rasterStateCI.lineWidth = 1.0f;

    VkPipelineColorBlendAttachmentState blendAttachmentState;
    blendAttachmentState.blendEnable = VK_FALSE;
    blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
    blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
    blendAttachmentState.colorWriteMask = 0xf;

    VkPipelineColorBlendStateCreateInfo colorBlendStateCI;
    colorBlendStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendStateCI.pNext = nullptr;
    colorBlendStateCI.flags = 0;
    colorBlendStateCI.logicOpEnable = VK_FALSE;
    colorBlendStateCI.logicOp = VK_LOGIC_OP_COPY;
    colorBlendStateCI.attachmentCount = 1;
    colorBlendStateCI.pAttachments = &blendAttachmentState;
    colorBlendStateCI.blendConstants[0] = 0.0f;
    colorBlendStateCI.blendConstants[1] = 0.0f;
    colorBlendStateCI.blendConstants[2] = 0.0f;
    colorBlendStateCI.blendConstants[3] = 0.0f;

    VkPipelineViewportStateCreateInfo viewportStateCI;
    viewportStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateCI.pNext = nullptr;
    viewportStateCI.flags = 0;
    viewportStateCI.viewportCount = 1;
    viewportStateCI.pViewports = nullptr; // Dynamic
    viewportStateCI.scissorCount = 1;
    viewportStateCI.pScissors = nullptr; // Dynamic

    vector<VkDynamicState> dynamicStateEnables_;
    dynamicStateEnables_ = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamicStateCI;
    dynamicStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateCI.pNext = nullptr;
    dynamicStateCI.flags = 0;
    dynamicStateCI.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables_.size());
    dynamicStateCI.pDynamicStates = dynamicStateEnables_.data();

    // Optimized depth settings for skybox: render at far plane with proper depth testing
    VkPipelineDepthStencilStateCreateInfo depthStencilStateCI;
    depthStencilStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilStateCI.pNext = nullptr;
    depthStencilStateCI.flags = 0;
    depthStencilStateCI.depthTestEnable = VK_TRUE;
    depthStencilStateCI.depthWriteEnable = VK_FALSE;
    depthStencilStateCI.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
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

    VkPipelineMultisampleStateCreateInfo multisampleStateCI;
    multisampleStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleStateCI.pNext = nullptr;
    multisampleStateCI.flags = 0;
    multisampleStateCI.rasterizationSamples = msaaSamples; // ex: VK_SAMPLE_COUNT_1_BIT
    multisampleStateCI.sampleShadingEnable = VK_FALSE;
    multisampleStateCI.minSampleShading = 1.0f;
    multisampleStateCI.pSampleMask = nullptr;
    multisampleStateCI.alphaToCoverageEnable = VK_FALSE;
    multisampleStateCI.alphaToOneEnable = VK_FALSE;

    VkPipelineRenderingCreateInfo pipelineRenderingCI;
    pipelineRenderingCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    pipelineRenderingCI.pNext = nullptr;
    pipelineRenderingCI.viewMask = 0;
    pipelineRenderingCI.colorAttachmentCount = static_cast<uint32_t>(outColorFormats.size());
    pipelineRenderingCI.pColorAttachmentFormats = outColorFormats.data();
    pipelineRenderingCI.depthAttachmentFormat = depthFormat;
    pipelineRenderingCI.stencilAttachmentFormat = depthFormat;

    VkGraphicsPipelineCreateInfo pipelineCI;
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
    pipelineCI.renderPass = VK_NULL_HANDLE;
    pipelineCI.subpass = 0;
    pipelineCI.basePipelineHandle = VK_NULL_HANDLE;
    pipelineCI.basePipelineIndex = -1;

    check(vkCreateGraphicsPipelines(device, ctx_.pipelineCache(), 1, &pipelineCI, nullptr,
                                    &pipeline_));
}

} // namespace hlab