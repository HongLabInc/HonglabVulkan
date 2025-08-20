#include "Pipeline.h"
#include "Vertex.h"

#include <glm/glm.hpp>
namespace hlab {

void Pipeline::createShadowMap()
{
    name_ = "shadowMap";

    const VkDevice device = ctx_.device();

    printLog("Creating a graphics pipeline: {}\n", name_);

    // 2. Create graphics pipeline
    vector<VkVertexInputAttributeDescription> vertexInputAttributes =
        Vertex::getAttributeDescriptions();

    vector<VkPipelineShaderStageCreateInfo> shaderStagesCI =
        shaderManager_.createPipelineShaderStageCIs(name_);

    // Vertex input configuration (same as forward pipeline but with Vertex structure)
    vector<VkVertexInputBindingDescription> vertexInputBindingDesc;
    vertexInputBindingDesc.resize(1);
    vertexInputBindingDesc[0].binding = 0;
    vertexInputBindingDesc[0].stride = sizeof(Vertex); // Use Vertex structure size
    vertexInputBindingDesc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexInputStateCI;
    vertexInputStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputStateCI.pNext = nullptr;
    vertexInputStateCI.flags = 0;
    vertexInputStateCI.vertexBindingDescriptionCount = uint32_t(vertexInputBindingDesc.size());
    vertexInputStateCI.pVertexBindingDescriptions = vertexInputBindingDesc.data();
    vertexInputStateCI.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(vertexInputAttributes.size());
    vertexInputStateCI.pVertexAttributeDescriptions = vertexInputAttributes.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI;
    inputAssemblyStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyStateCI.pNext = nullptr;
    inputAssemblyStateCI.flags = 0;
    inputAssemblyStateCI.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyStateCI.primitiveRestartEnable = VK_FALSE;

    // Rasterization state with depth bias to prevent shadow acne
    VkPipelineRasterizationStateCreateInfo rasterStateCI;
    rasterStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterStateCI.pNext = nullptr;
    rasterStateCI.flags = 0;
    rasterStateCI.depthClampEnable = VK_TRUE; // Enable depth clamping for shadow maps
    rasterStateCI.rasterizerDiscardEnable = VK_FALSE;
    rasterStateCI.polygonMode = VK_POLYGON_MODE_FILL;
    rasterStateCI.cullMode = VK_CULL_MODE_NONE; // No culling for shadows to prevent peter panning
    rasterStateCI.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterStateCI.depthBiasEnable = VK_TRUE;      // Enable depth bias
    rasterStateCI.depthBiasConstantFactor = 1.1f; // Constant bias
    rasterStateCI.depthBiasClamp = 0.0f;
    rasterStateCI.depthBiasSlopeFactor = 2.0f; // Slope-scaled bias
    rasterStateCI.lineWidth = 1.0f;

    // No color blending needed for depth-only pass
    VkPipelineColorBlendStateCreateInfo colorBlendStateCI;
    colorBlendStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendStateCI.pNext = nullptr;
    colorBlendStateCI.flags = 0;
    colorBlendStateCI.logicOpEnable = VK_FALSE;
    colorBlendStateCI.logicOp = VK_LOGIC_OP_COPY;
    colorBlendStateCI.attachmentCount = 0; // No color attachments
    colorBlendStateCI.pAttachments = nullptr;
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

    vector<VkDynamicState> dynamicStateEnables = {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_DEPTH_BIAS // Allow dynamic depth bias adjustment
    };

    VkPipelineDynamicStateCreateInfo dynamicStateCI;
    dynamicStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateCI.pNext = nullptr;
    dynamicStateCI.flags = 0;
    dynamicStateCI.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());
    dynamicStateCI.pDynamicStates = dynamicStateEnables.data();

    // Depth testing configuration for shadow map generation
    VkPipelineDepthStencilStateCreateInfo depthStencilStateCI;
    depthStencilStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilStateCI.pNext = nullptr;
    depthStencilStateCI.flags = 0;
    depthStencilStateCI.depthTestEnable = VK_TRUE;
    depthStencilStateCI.depthWriteEnable = VK_TRUE;
    depthStencilStateCI.depthCompareOp = VK_COMPARE_OP_LESS;
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

    // No multisampling for shadow maps (better performance and quality)
    VkPipelineMultisampleStateCreateInfo multisampleStateCI;
    multisampleStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleStateCI.pNext = nullptr;
    multisampleStateCI.flags = 0;
    multisampleStateCI.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampleStateCI.sampleShadingEnable = VK_FALSE;
    multisampleStateCI.minSampleShading = 1.0f;
    multisampleStateCI.pSampleMask = nullptr;
    multisampleStateCI.alphaToCoverageEnable = VK_FALSE;
    multisampleStateCI.alphaToOneEnable = VK_FALSE;

    // Pipeline rendering info for depth-only pass
    VkPipelineRenderingCreateInfo pipelineRenderingCI;
    pipelineRenderingCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    pipelineRenderingCI.pNext = nullptr;
    pipelineRenderingCI.viewMask = 0;
    pipelineRenderingCI.colorAttachmentCount = 0; // No color attachments
    pipelineRenderingCI.pColorAttachmentFormats = nullptr;
    pipelineRenderingCI.depthAttachmentFormat = VK_FORMAT_D16_UNORM;   // Shadow map format
    pipelineRenderingCI.stencilAttachmentFormat = VK_FORMAT_UNDEFINED; // No stencil for shadows

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