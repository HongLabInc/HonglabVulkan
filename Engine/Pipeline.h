#pragma once

#include "ShaderManager.h"
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

    Pipeline(Context& ctx, ShaderManager& shaderManager, string pipelineName,
             VkFormat outColorFormat, VkFormat depthFormat, VkSampleCountFlagBits msaaSamples)
        : ctx_(ctx), shaderManager_(shaderManager)
    {
        createByName(pipelineName, outColorFormat, depthFormat, msaaSamples);
    }

    Pipeline(Pipeline&& other) noexcept
        : ctx_(other.ctx_), name_(std::move(other.name_)), pipelineLayout_(other.pipelineLayout_),
          pipeline_(other.pipeline_), shaderManager_(other.shaderManager_)
    {
        other.pipelineLayout_ = VK_NULL_HANDLE;
        other.pipeline_ = VK_NULL_HANDLE;
    }

    Pipeline& operator=(Pipeline&& other) noexcept
    {
        if (this != &other) {
            cleanup();
            name_ = std::move(other.name_);
            pipelineLayout_ = other.pipelineLayout_;
            pipeline_ = other.pipeline_;
            other.pipelineLayout_ = VK_NULL_HANDLE;
            other.pipeline_ = VK_NULL_HANDLE;
            other.name_.clear();
        }
        return *this;
    }

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    ~Pipeline()
    {
        cleanup();
    }

    void cleanup();
    void createByName(string pipelineName, optional<VkFormat> outColorFormat = nullopt,
                      optional<VkFormat> depthFormat = nullopt,
                      optional<VkSampleCountFlagBits> msaaSamples = nullopt);
    void createCommon();
    void createCompute();
    void createPost(VkFormat outColorFormat, VkFormat depthFormat);
    void createGui(VkFormat outColorFormat);
    void createSky(VkFormat outColorFormat, VkFormat depthFormat,
                   VkSampleCountFlagBits msaaSamples);
    void createShadowMap();
    void createPbrForward(VkFormat outColorFormat, VkFormat depthFormat,
                          VkSampleCountFlagBits msaaSamples);
    void createPbrDeferred();
    void createSsao();
    void createTriangle(VkFormat outColorFormat);

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
};

} // namespace hlab