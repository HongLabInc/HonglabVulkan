#pragma once

#include "ShaderManager.h"
#include "PipelineConfig.h"
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

class DescriptorSet;

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

    void createFromConfig(const PipelineConfig& config, optional<VkFormat> outColorFormat = nullopt,
                          optional<VkFormat> depthFormat = nullopt,
                          optional<VkSampleCountFlagBits> msaaSamples = nullopt);

    void createCommon();
    void createCompute();

    void recordCommands(const VkCommandBuffer& cmd)
    {
    }

    void dispatch(const VkCommandBuffer& cmd, uint32_t frameIndex)
    {
        assert(bindPoint_ == VK_PIPELINE_BIND_POINT_COMPUTE);

        submitBarriers(cmd, frameIndex);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);

        bindDescriptorSets(cmd, frameIndex);

        // Use actual local workgroup size from shader reflection instead of hardcoded values
        uint32_t groupCountX = (width_ + local_size_[0] - 1) / local_size_[0];
        uint32_t groupCountY = (height_ + local_size_[1] - 1) / local_size_[1];
        vkCmdDispatch(cmd, groupCountX, groupCountY, 1);
    }

    auto pipeline() const -> VkPipeline;
    auto pipelineLayout() const -> VkPipelineLayout;
    auto shaderManager() -> ShaderManager&;
    auto layouts() -> vector<VkDescriptorSetLayout>&
    {
        return layouts_;
    }

    auto bindingInfos() const -> const vector<vector<BindingInfo>>&
    {
        return bindingInfos_;
    }

    void setDescriptorSets(vector<vector<reference_wrapper<DescriptorSet>>>& descriptorSets);

    void bindDescriptorSets(const VkCommandBuffer& cmd, uint32_t frameIndex)
    {
        assert(frameIndex < descriptorSetHandles_.size() && "Frame index out of bounds");
        assert(!descriptorSetHandles_[frameIndex].empty() && "No descriptor sets for this frame");

        vkCmdBindDescriptorSets(cmd, bindPoint_, pipelineLayout_, 0,
                                static_cast<uint32_t>(descriptorSetHandles_[frameIndex].size()),
                                descriptorSetHandles_[frameIndex].data(), 0, nullptr);
    }

    void submitBarriers(const VkCommandBuffer& cmd, uint32_t frameIndex);

  private:
    Context& ctx_;
    ShaderManager& shaderManager_;

    VkPipelineLayout pipelineLayout_{VK_NULL_HANDLE};
    VkPipeline pipeline_{VK_NULL_HANDLE};
    VkPipelineBindPoint bindPoint_{VK_PIPELINE_BIND_POINT_GRAPHICS};

    string name_{};
    vector<VkDescriptorSetLayout> layouts_{};
    vector<vector<reference_wrapper<DescriptorSet>>> descriptorSets_; // [frameNumber][setIndex]
    vector<vector<VkDescriptorSet>> descriptorSetHandles_;            // [frameNumber][setIndex]
    vector<vector<BindingInfo>> bindingInfos_;

    uint32_t width_{0};
    uint32_t height_{0};

    array<uint32_t, 3> local_size_{1, 1, 1}; // [x, y, z] - Initialize from shaderManager when this pipeline created (only if this pipeline is compute)

    void validateRequiredFormats(const PipelineConfig& config, optional<VkFormat> outColorFormat,
                                 optional<VkFormat> depthFormat,
                                 optional<VkSampleCountFlagBits> msaaSamples);

    void createGraphicsFromConfig(const PipelineConfig& config, VkFormat outColorFormat,
                                  optional<VkFormat> depthFormat,
                                  optional<VkSampleCountFlagBits> msaaSamples);

    // Helper method to determine pipeline dimensions from first write-only binding
    void determineDimensionsFromFirstWriteOnlyBinding();

    // Helper method to initialize compute shader local workgroup size
    void initializeComputeLocalWorkgroupSize();
};

} // namespace hlab