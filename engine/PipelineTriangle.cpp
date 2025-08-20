#include "Pipeline.h"

namespace hlab {

void Pipeline::createTriangle(VkFormat outColorFormat)
{
    const uint32_t sizeofVertexStructure = 0; // No vertex input
    const VkDevice device = ctx_.device();

    printLog("Creating a graphics pipeline: {}\n", name_);

    // Fixed vertex input - no vertex buffers needed for procedural triangle
    VkPipelineVertexInputStateCreateInfo vertexInputCI{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInputCI.vertexBindingDescriptionCount = 0; // No vertex buffer bindings
    vertexInputCI.pVertexBindingDescriptions = nullptr;
    vertexInputCI.vertexAttributeDescriptionCount = 0; // No vertex attributes
    vertexInputCI.pVertexAttributeDescriptions = nullptr;

    // Input assembly - triangle list topology
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyCI{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssemblyCI.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyCI.primitiveRestartEnable = VK_FALSE;

    // Viewport state - using dynamic viewport and scissor
    VkPipelineViewportStateCreateInfo viewportStateCI{
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportStateCI.viewportCount = 1;
    viewportStateCI.pViewports = nullptr; // Dynamic
    viewportStateCI.scissorCount = 1;
    viewportStateCI.pScissors = nullptr; // Dynamic

    // Rasterization state
    VkPipelineRasterizationStateCreateInfo rasterizationCI{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizationCI.depthClampEnable = VK_FALSE;
    rasterizationCI.rasterizerDiscardEnable = VK_FALSE;
    rasterizationCI.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationCI.lineWidth = 1.0f;
    rasterizationCI.cullMode = VK_CULL_MODE_NONE;
    rasterizationCI.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationCI.depthBiasEnable = VK_FALSE;

    // Multisample state - no multisampling
    VkPipelineMultisampleStateCreateInfo multisampleCI{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampleCI.sampleShadingEnable = VK_FALSE;
    multisampleCI.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth stencil state - no depth/stencil testing for 2D triangle
    VkPipelineDepthStencilStateCreateInfo depthStencilCI{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depthStencilCI.depthTestEnable = VK_FALSE;
    depthStencilCI.depthWriteEnable = VK_FALSE;
    depthStencilCI.depthCompareOp = VK_COMPARE_OP_ALWAYS;
    depthStencilCI.depthBoundsTestEnable = VK_FALSE;
    depthStencilCI.stencilTestEnable = VK_FALSE;

    // Color blend state - simple replacement, no blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlendCI{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlendCI.logicOpEnable = VK_FALSE;
    colorBlendCI.logicOp = VK_LOGIC_OP_COPY;
    colorBlendCI.attachmentCount = 1;
    colorBlendCI.pAttachments = &colorBlendAttachment;

    // Dynamic state - viewport and scissor
    vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicStateCI{
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicStateCI.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicStateCI.pDynamicStates = dynamicStates.data();

    // Get shader stages from shader manager
    vector<VkPipelineShaderStageCreateInfo> shaderStages =
        shaderManager_.createPipelineShaderStageCIs(name_);

    // Dynamic rendering setup for Vulkan 1.3
    VkPipelineRenderingCreateInfo pipelineRenderingCI{
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    pipelineRenderingCI.colorAttachmentCount = 1;
    pipelineRenderingCI.pColorAttachmentFormats = &outColorFormat;
    pipelineRenderingCI.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    pipelineRenderingCI.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    // Create graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineCI{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipelineCI.pNext = &pipelineRenderingCI;
    pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineCI.pStages = shaderStages.data();
    pipelineCI.pVertexInputState = &vertexInputCI;
    pipelineCI.pInputAssemblyState = &inputAssemblyCI;
    pipelineCI.pTessellationState = nullptr;
    pipelineCI.pViewportState = &viewportStateCI;
    pipelineCI.pRasterizationState = &rasterizationCI;
    pipelineCI.pMultisampleState = &multisampleCI;
    pipelineCI.pDepthStencilState = &depthStencilCI;
    pipelineCI.pColorBlendState = &colorBlendCI;
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