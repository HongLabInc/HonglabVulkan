#pragma once

#include "Shader.h"
#include "Context.h"
#include <vector>
#include <unordered_map>
#include <initializer_list>
#include <map>

namespace hlab {

using namespace std;

struct BindingInfo
{
    string resourceName{};             // name of binding resource
    uint32_t setIndex{0};              // descriptor set index (0, 1, 2, ...)
    uint32_t bindingIndex{0};          // binding index within the set (0, 1, 2, ...)
    VkImageLayout targetLayout;        // required image layout for this binding
    VkAccessFlags2 targetAccess;       // required access flags for this binding
    VkPipelineStageFlags2 targetStage; // pipeline stage where this binding is used
    bool writeonly{false};
};

class ShaderManager
{
  public:
    ShaderManager(Context& ctx, string shaderPathPrefix,
                  const initializer_list<pair<string, vector<string>>>& pipelineShaders);

    ShaderManager(const ShaderManager&) = delete;
    ShaderManager& operator=(const ShaderManager&) = delete;
    ShaderManager& operator=(ShaderManager&&) = delete;

    void cleanup();

    auto createPipelineShaderStageCIs(string pipelineName) const
        -> vector<VkPipelineShaderStageCreateInfo>;
    auto createVertexInputAttrDesc(string pipelineName) const
        -> vector<VkVertexInputAttributeDescription>;
    auto pushConstantsRange(string pipelineName) -> VkPushConstantRange;

    // Get local workgroup size for compute pipelines
    auto getComputeLocalWorkgroupSize(string pipelineName) const -> array<uint32_t, 3>;

    // Accessors
    auto pipelineShaders() const -> const unordered_map<string, vector<Shader>>&;
    const vector<LayoutInfo>& layoutInfos() const;
    const unordered_map<string, vector<vector<BindingInfo>>>& bindingInfos() const;

  private:
    // Member variables
    Context& ctx_;
    unordered_map<string, vector<Shader>> pipelineShaders_;
    unordered_map<string, vector<vector<BindingInfo>>> bindingInfos_;
    vector<LayoutInfo> layoutInfos_;

    // Private helper methods
    void createFromShaders(string shaderPathPrefix,
                           initializer_list<pair<string, vector<string>>> pipelineShaders);
    void collectLayoutInfos();
    void collectPerPipelineBindings(
        const string& pipelineName,
        map<uint32_t, map<uint32_t, VkDescriptorSetLayoutBinding>>& bindingCollector) const;
    auto createLayoutBindingFromReflect(const SpvReflectDescriptorBinding* binding,
                                        VkShaderStageFlagBits shaderStage) const
        -> VkDescriptorSetLayoutBinding;
};

} // namespace hlab
