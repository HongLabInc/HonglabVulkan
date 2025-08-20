#include "Pipeline.h"

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

void Pipeline::createByName(string pipelineName, optional<VkFormat> outColorFormat,
                            optional<VkFormat> depthFormat,
                            optional<VkSampleCountFlagBits> msaaSamples)
{
    name_ = pipelineName;

    createCommon();

    if (name_ == "compute") {
        createCompute();
    } else if (name_ == "triangle") {
        createTriangle(outColorFormat.value());
    } else if (name_ == "post") {
        if (outColorFormat.has_value() && depthFormat.has_value()) {
            createPost(outColorFormat.value(), depthFormat.value());
        } else {
            exitWithMessage("outColorFormat and depthFormat required for {}", name_);
        }
    } else if (name_ == "gui") {
        if (outColorFormat.has_value()) {
            createGui(outColorFormat.value());
        } else {
            exitWithMessage("outColorFormat required for {}", name_);
        }
    } else if (name_ == "sky") {
        if (outColorFormat.has_value() && depthFormat.has_value() && msaaSamples.has_value()) {
            createSky(outColorFormat.value(), depthFormat.value(), msaaSamples.value());
        } else {
            exitWithMessage("outColorFormat, depthFormat, and msaaSamples required for {}", name_);
        }
    } else if (name_ == "shadowMap") {
        createShadowMap();
    } else if (name_ == "pbrForward") {
        if (outColorFormat.has_value() && depthFormat.has_value() && msaaSamples.has_value()) {
            createPbrForward(outColorFormat.value(), depthFormat.value(), msaaSamples.value());
        } else {
            exitWithMessage("outColorFormat, depthFormat, and msaaSamples required for {}", name_);
        }
    } else if (name_ == "pbrDeferred") {
        createPbrDeferred();
    } else if (name_ == "ssao") {
        createSsao();
    } else {
        exitWithMessage("Pipeline name not available: {}", pipelineName);
    }
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