#include "Pipeline.h"

namespace hlab {

void Pipeline::createCompute()
{
    printLog("Creating compute pipeline: {}\n", name_);

    const VkDevice device = ctx_.device();

    // Get shader stage create info for the compute shader
    vector<VkPipelineShaderStageCreateInfo> shaderStagesCI =
        shaderManager_.createPipelineShaderStageCIs(name_);

    if (shaderStagesCI.empty()) {
        exitWithMessage("No compute shader stages found for pipeline: {}", name_);
    }

    if (shaderStagesCI.size() != 1) {
        exitWithMessage("Compute pipeline must have exactly one shader stage, found: {}",
                        shaderStagesCI.size());
    }

    if (shaderStagesCI[0].stage != VK_SHADER_STAGE_COMPUTE_BIT) {
        exitWithMessage("Expected compute shader stage, but got different stage type");
    }

    // Create compute pipeline
    VkComputePipelineCreateInfo pipelineCI{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    pipelineCI.layout = pipelineLayout_;
    pipelineCI.stage = shaderStagesCI[0]; // Only one shader stage for compute
    pipelineCI.basePipelineHandle = VK_NULL_HANDLE;
    pipelineCI.basePipelineIndex = -1;

    check(vkCreateComputePipelines(device, ctx_.pipelineCache(), 1, &pipelineCI, nullptr,
                                   &pipeline_));

    printLog("Successfully created compute pipeline\n");
}

} // namespace hlab