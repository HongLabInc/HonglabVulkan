#include "ShaderManager.h"
#include "VulkanTools.h"
#include "Logger.h"

// Per-pipeline binding definitions
#include "ShadowMapPipeline.h"
#include "PbrDeferredPipeline.h"
#include "SkyPipeline.h"
#include "DeferredLightingPipeline.h"
#include "PostPipeline.h"
#include "GuiPipeline.h"
#include "TrianglePipeline.h"
#include "ComputePipeline.h"

#include <string>
#include <algorithm>
#include <unordered_map>

namespace hlab {

using namespace std;

// ============================================================================
// Pipeline definition: layout bindings + binding infos + push constants + local size
// Each per-pipeline class (e.g. ShadowMapPipeline) provides these via static methods.
// ============================================================================

// Data needed from each pipeline class for layout construction
struct PipelineBindingDef
{
    vector<vector<VkDescriptorSetLayoutBinding>> layoutBindings;
    vector<vector<BindingInfo>> bindingInfos;
};

static auto getBindingDef(const string& name) -> PipelineBindingDef
{
    if (name == "shadowMap")
        return {ShadowMapPipeline::layoutBindings(), ShadowMapPipeline::bindingInfos()};
    if (name == "pbrDeferred")
        return {PbrDeferredPipeline::layoutBindings(), PbrDeferredPipeline::bindingInfos()};
    if (name == "sky")
        return {SkyPipeline::layoutBindings(), SkyPipeline::bindingInfos()};
    if (name == "deferredLighting")
        return {DeferredLightingPipeline::layoutBindings(),
                DeferredLightingPipeline::bindingInfos()};
    if (name == "post")
        return {PostPipeline::layoutBindings(), PostPipeline::bindingInfos()};
    if (name == "gui")
        return {GuiPipeline::layoutBindings(), GuiPipeline::bindingInfos()};
    if (name == "triangle")
        return {TrianglePipeline::layoutBindings(), TrianglePipeline::bindingInfos()};
    if (name == "compute")
        return {ComputePipeline::layoutBindings(), ComputePipeline::bindingInfos()};

    exitWithMessage("No binding definitions for pipeline '{}'", name);
    return {};
}

static auto getPushConstantRange(const string& name) -> VkPushConstantRange
{
    if (name == "shadowMap") return ShadowMapPipeline::pushConstantRange();
    if (name == "pbrDeferred") return PbrDeferredPipeline::pushConstantRange();
    if (name == "sky") return SkyPipeline::pushConstantRange();
    if (name == "deferredLighting") return DeferredLightingPipeline::pushConstantRange();
    if (name == "post") return PostPipeline::pushConstantRange();
    if (name == "gui") return GuiPipeline::pushConstantRange();
    if (name == "triangle") return TrianglePipeline::pushConstantRange();

    printLog("[Warning] No push constant range defined for pipeline '{}'", name);
    return {0, 0, 0};
}

static auto getLocalWorkgroupSize(const string& name) -> array<uint32_t, 3>
{
    if (name == "deferredLighting") return DeferredLightingPipeline::kLocalSize;
    if (name == "compute") return ComputePipeline::kLocalSize;

    printLog("[Warning] No compute workgroup size defined for pipeline '{}'", name);
    return {1, 1, 1};
}

// ============================================================================

ShaderManager::ShaderManager(Context& ctx, string shaderPathPrefix,
                             const initializer_list<pair<string, vector<string>>>& pipelineShaders)
    : ctx_(ctx)
{
    createFromShaders(shaderPathPrefix, pipelineShaders);

    buildLayoutInfos();

    ctx_.descriptorPool().createLayouts(layoutInfos_);
}

void ShaderManager::buildLayoutInfos()
{
    bindingInfos_.clear();
    layoutInfos_.clear();

    // Layout sharing: multiple pipelines can share the same VkDescriptorSetLayout
    // if their bindings have the same structure (type, count) regardless of stageFlags.
    // This map tracks: binding structure (stageFlags zeroed) -> index in layoutInfos_
    unordered_map<vector<VkDescriptorSetLayoutBinding>, size_t, BindingHash, BindingEqual>
        existingLayouts;

    for (const auto& [pipelineName, shaders] : pipelineShaders_) {
        auto def = getBindingDef(pipelineName);

        bindingInfos_[pipelineName] = move(def.bindingInfos);

        for (uint32_t setIdx = 0; setIdx < def.layoutBindings.size(); ++setIdx) {
            auto& layoutBindings = def.layoutBindings[setIdx];

            // Zero stageFlags for structural comparison (same type+count = same layout)
            auto key = layoutBindings;
            for (auto& b : key) {
                b.stageFlags = 0;
            }

            auto [it, inserted] = existingLayouts.try_emplace(key, layoutInfos_.size());
            if (inserted) {
                // New layout — register it
                LayoutInfo info;
                info.bindings_ = layoutBindings;
                info.pipelineNamesAndSetNumbers_.emplace_back(pipelineName, setIdx);
                layoutInfos_.push_back(move(info));
            } else {
                // Layout already exists — share it, merge stageFlags per-binding
                auto& existing = layoutInfos_[it->second];
                for (size_t i = 0; i < existing.bindings_.size(); ++i) {
                    existing.bindings_[i].stageFlags |= layoutBindings[i].stageFlags;
                }
                existing.pipelineNamesAndSetNumbers_.emplace_back(pipelineName, setIdx);
            }
        }
    }

    // Debug log
    printLog("\n=== Shader Manager Binding Information (Explicit) ===");
    for (const auto& [pipelineName, pipelineBindingInfos] : bindingInfos_) {
        printLog("Pipeline '{}': {} sets", pipelineName, pipelineBindingInfos.size());
        for (size_t setIdx = 0; setIdx < pipelineBindingInfos.size(); ++setIdx) {
            const auto& setBindings = pipelineBindingInfos[setIdx];
            if (!setBindings.empty()) {
                printLog("  Set {}: {} bindings", setIdx, setBindings.size());
                for (size_t b = 0; b < setBindings.size(); ++b) {
                    const auto& bi = setBindings[b];
                    if (!bi.resourceName.empty()) {
                        printLog("    Binding {}: name='{}', set={}, binding={}, writeonly={}",
                                 b, bi.resourceName, bi.setIndex, bi.bindingIndex,
                                 bi.writeonly ? "true" : "false");
                    }
                }
            }
        }
    }
    printLog("=====================================================\n");
}

void ShaderManager::createFromShaders(
    string shaderPathPrefix, initializer_list<pair<string, vector<string>>> pipelineShaders)
{
    for (const auto& [pipelineName, shaderFilenames] : pipelineShaders) {
        vector<Shader>& shaders = pipelineShaders_[pipelineName];
        shaders.reserve(shaderFilenames.size());

        for (string filename : shaderFilenames) {
            filename = shaderPathPrefix + filename;
            if (filename.substr(filename.length() - 4) != ".spv") {
                filename += ".spv";
            }
            shaders.emplace_back(Shader(ctx_, filename));
        }
    }
}

void ShaderManager::cleanup()
{
    for (auto& [pipelineName, shaders] : pipelineShaders_) {
        for (auto& shader : shaders) {
            shader.cleanup();
        }
    }
    pipelineShaders_.clear();
}

vector<VkPipelineShaderStageCreateInfo>
ShaderManager::createPipelineShaderStageCIs(const string& pipelineName) const
{
    const auto& shaders = pipelineShaders_.at(pipelineName);

    vector<VkPipelineShaderStageCreateInfo> shaderStages;
    for (const auto& shader : shaders) {
        VkPipelineShaderStageCreateInfo stageCI = {};
        stageCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageCI.stage = shader.stage_;
        stageCI.module = shader.shaderModule_;
        stageCI.pName = "main";
        stageCI.pSpecializationInfo = nullptr;
        shaderStages.push_back(stageCI);
    }

    return shaderStages;
}

VkPushConstantRange ShaderManager::pushConstantsRange(const string& pipelineName) const
{
    return getPushConstantRange(pipelineName);
}

array<uint32_t, 3> ShaderManager::getComputeLocalWorkgroupSize(const string& pipelineName) const
{
    return getLocalWorkgroupSize(pipelineName);
}

const unordered_map<string, vector<Shader>>& ShaderManager::pipelineShaders() const
{
    return pipelineShaders_;
}

const vector<LayoutInfo>& ShaderManager::layoutInfos() const
{
    return layoutInfos_;
}

const unordered_map<string, vector<vector<BindingInfo>>>& ShaderManager::bindingInfos() const
{
    return bindingInfos_;
}

} // namespace hlab
