#pragma once

#include "ShaderManager.h"
#include "PipelineConfig.h" // Add this include
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <span>
#include <array>
#include <functional>
#include <optional>

// 안내: cpp 파일은 여러 개로 나뉘어 있습니다. (Pipeline.cpp, PipelineCompute.cpp, ...)

namespace hlab {

using namespace std;

class Pipeline
{
  public:
    Pipeline(Context& ctx, ShaderManager& shaderManager) : ctx_(ctx), shaderManager_(shaderManager)
    {
    }

    // PipelineConfig-based constructor
    Pipeline(Context& ctx, ShaderManager& shaderManager, const PipelineConfig& config,
             optional<VkFormat> outColorFormat = nullopt, optional<VkFormat> depthFormat = nullopt,
             optional<VkSampleCountFlagBits> msaaSamples = nullopt)
        : ctx_(ctx), shaderManager_(shaderManager)
    {
        createFromConfig(config, outColorFormat, depthFormat, msaaSamples);
    }

    // Delete copy and move constructors/operators since we'll use unique_ptr
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    Pipeline(Pipeline&&) = delete;
    Pipeline& operator=(Pipeline&&) = delete;

    ~Pipeline()
    {
        cleanup();
    }

    void cleanup();

    // NEW: Config-based creation method
    void createFromConfig(const PipelineConfig& config, optional<VkFormat> outColorFormat = nullopt,
                          optional<VkFormat> depthFormat = nullopt,
                          optional<VkSampleCountFlagBits> msaaSamples = nullopt);

    void createCommon();
    void createCompute();

    // void dispatch(const VkCommandBuffer& cmd,
    //               initializer_list<reference_wrapper<const DescriptorSet>> descriptorSets,
    //               uint32_t groupCountX, uint32_t groupCountY)
    //{
    //     vector<VkDescriptorSet> vkDesSets;
    //     vkDesSets.reserve(descriptorSets.size());

    //    // Extract actual VkDescriptorSet handles
    //    for (const auto& descSetRef : descriptorSets) {
    //        vkDesSets.push_back(descSetRef.get().handle());
    //    }

    //    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);
    //    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout_, 0,
    //                            uint32_t(vkDesSets.size()), vkDesSets.data(), 0, nullptr);
    //    vkCmdDispatch(cmd, groupCountX, groupCountY, 1);
    //}

    // void draw(const VkCommandBuffer& cmd,
    //           initializer_list<reference_wrapper<const DescriptorSet>> descriptorSets,
    //           uint32_t vertexCount)
    //{
    //     vector<VkDescriptorSet> vkDesSets;
    //     vkDesSets.reserve(descriptorSets.size());

    //    // Extract actual VkDescriptorSet handles
    //    for (const auto& descSetRef : descriptorSets) {
    //        vkDesSets.push_back(descSetRef.get().handle());
    //    }

    //    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    //    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0,
    //                            uint32_t(vkDesSets.size()), vkDesSets.data(), 0, nullptr);
    //    vkCmdDraw(cmd, vertexCount, 1, 0, 0);
    //}

    auto pipeline() const -> VkPipeline;
    auto pipelineLayout() const -> VkPipelineLayout;
    auto shaderManager() -> ShaderManager&;

  private:
    Context& ctx_;
    ShaderManager& shaderManager_;

    VkPipelineLayout pipelineLayout_{VK_NULL_HANDLE};
    VkPipeline pipeline_{VK_NULL_HANDLE};

    string name_{};

    // NEW: Helper methods for config-based creation
    void validateRequiredFormats(const PipelineConfig& config, optional<VkFormat> outColorFormat,
                                 optional<VkFormat> depthFormat,
                                 optional<VkSampleCountFlagBits> msaaSamples);

    void createGraphicsFromConfig(const PipelineConfig& config, VkFormat outColorFormat,
                                  optional<VkFormat> depthFormat,
                                  optional<VkSampleCountFlagBits> msaaSamples);
};

} // namespace hlab