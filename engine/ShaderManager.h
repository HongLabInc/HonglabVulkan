#pragma once

#include "Shader.h"
#include "BindingInfo.h"
#include "Context.h"
#include <vector>
#include <unordered_map>
#include <initializer_list>
#include <map>

namespace hlab {

using namespace std;

class ShaderManager
{
  public:
    ShaderManager(Context& ctx, string shaderPathPrefix,
                  const initializer_list<pair<string, vector<string>>>& pipelineShaders);

    ShaderManager(const ShaderManager&) = delete;
    ShaderManager& operator=(const ShaderManager&) = delete;
    ShaderManager& operator=(ShaderManager&&) = delete;

    void cleanup();

    auto createPipelineShaderStageCIs(const string& pipelineName) const
        -> vector<VkPipelineShaderStageCreateInfo>;
    auto pushConstantsRange(const string& pipelineName) const -> VkPushConstantRange;

    // Get local workgroup size for compute pipelines
    auto getComputeLocalWorkgroupSize(const string& pipelineName) const -> array<uint32_t, 3>;

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
    void buildLayoutInfos();
};

} // namespace hlab
