#include "Pipeline.h"

namespace hlab {

void Pipeline::createCompute()
{
    // vector<VkPipelineShaderStageCreateInfo> shaderStagesCI =
    //     shaderManager_.createPipelineShaderStageCIs();

    // VkComputePipelineCreateInfo pipelineCI{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    // pipelineCI.layout = pipelineLayout_;
    // pipelineCI.stage = shaderStagesCI[0]; // Only one shader stage

    // check(vkCreateComputePipelines(ctx_.device(), ctx_.pipelineCache(), 1, &pipelineCI, nullptr,
    //                                &pipeline_));
}

} // namespace hlab