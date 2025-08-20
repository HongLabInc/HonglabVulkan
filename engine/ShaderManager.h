#pragma once

#include "Shader.h"
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
    auto createPipelineShaderStageCIs(string pipelineName) const
        -> vector<VkPipelineShaderStageCreateInfo>;

    auto pushConstantsRange(string pipelineName) -> VkPushConstantRange
    {
        const auto& shaders = pipelineShaders_.at(pipelineName);

        // Search through all shaders in the pipeline for push constants
        for (const auto& shader : shaders) {
            const auto& reflectModule = shader.reflectModule_;

            // Check if this shader has push constants
            if (reflectModule.push_constant_block_count > 0) {
                const SpvReflectBlockVariable* pushBlock = &reflectModule.push_constant_blocks[0];

                VkPushConstantRange pushConstantRange{};
                pushConstantRange.stageFlags = static_cast<VkShaderStageFlags>(shader.stage_);
                pushConstantRange.offset = 0;
                pushConstantRange.size = pushBlock->size;

                // Accumulate stage flags from other shaders that also use push constants
                for (const auto& otherShader : shaders) {
                    if (otherShader.reflectModule_.push_constant_block_count > 0) {
                        pushConstantRange.stageFlags |=
                            static_cast<VkShaderStageFlags>(otherShader.stage_);
                    }
                }

                return pushConstantRange;
            }
        }

        // Return empty range if no push constants found
        VkPushConstantRange emptyRange{};
        emptyRange.stageFlags = 0;
        emptyRange.offset = 0;
        emptyRange.size = 0;
        return emptyRange;
    }

    auto createVertexInputAttrDesc(string pipelineName) const
        -> vector<VkVertexInputAttributeDescription>;
    auto collectPerPipelineBindings() const -> vector<VkDescriptorSetLayoutBinding>;
    auto pipelineShaders() const -> const unordered_map<string, vector<Shader>>&
    {
        return pipelineShaders_;
    }

    const vector<LayoutInfo>& layoutInfos() const
    {
        return layoutInfos_;
    }

  private:
    Context& ctx_;
    unordered_map<string, vector<Shader>> pipelineShaders_;
    vector<LayoutInfo> layoutInfos_;

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
