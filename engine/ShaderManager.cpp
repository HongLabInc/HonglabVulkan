#include "ShaderManager.h"
#include "VulkanTools.h"
#include "Logger.h"
#include <string>
#include <algorithm>
#include <unordered_map>

namespace hlab {

using namespace std;

ShaderManager::ShaderManager(Context& ctx, string shaderPathPrefix,
                             const initializer_list<pair<string, vector<string>>>& pipelineShaders)
    : ctx_(ctx)
{
    createFromShaders(shaderPathPrefix, pipelineShaders);

    collectLayoutInfos();

    ctx_.descriptorPool().createLayouts(layoutInfos_);
}

void ShaderManager::collectLayoutInfos()
{
    // Use unordered_map with custom hash and equality functors
    unordered_map<vector<VkDescriptorSetLayoutBinding>, vector<tuple<string, uint32_t>>,
                  BindingHash, BindingEqual>
        bindingsCollector;

    for (const auto& [pipelineName, shaders] : pipelineShaders_) {
        map<uint32_t, map<uint32_t, VkDescriptorSetLayoutBinding>> perPipelineBindings;
        collectPerPipelineBindings(pipelineName, perPipelineBindings);

        for (const auto& [setIndex, bindingsMap] : perPipelineBindings) {
            if (bindingsMap.empty())
                continue;

            vector<VkDescriptorSetLayoutBinding> bindingsVector;
            bindingsVector.reserve(bindingsMap.size());
            for (const auto& [bindingIndex, layoutBinding] : bindingsMap) {
                bindingsVector.push_back(layoutBinding);
            }

            // Create normalized bindings for comparison (without stage flags)
            vector<VkDescriptorSetLayoutBinding> normalizedBindings = bindingsVector;
            VkShaderStageFlags accumulatedStageFlags = 0;

            // Collect all stage flags for this set of bindings
            for (const auto& binding : bindingsVector) {
                accumulatedStageFlags |= binding.stageFlags;
            }

            // Remove stage flags from normalized bindings for proper comparison
            for (auto& binding : normalizedBindings) {
                binding.stageFlags = 0;
            }

            auto [it, inserted] = bindingsCollector.try_emplace(
                normalizedBindings,
                vector<tuple<string, uint32_t>>{make_tuple(pipelineName, setIndex)});
            if (!inserted) {
                // Accumulate stage flags from existing entry
                for (size_t i = 0; i < it->first.size(); ++i) {
                    accumulatedStageFlags |= it->first[i].stageFlags;
                }
                it->second.emplace_back(pipelineName, setIndex);
            }

            // Update the key with accumulated stage flags (this is a bit tricky with unordered_map)
            // We need to modify the bindings in place since the key is const
            auto& keyBindings = const_cast<vector<VkDescriptorSetLayoutBinding>&>(it->first);
            for (auto& binding : keyBindings) {
                binding.stageFlags = accumulatedStageFlags;
            }
        }
    }

    // Convert bindingsCollector to layoutInfos_
    layoutInfos_.clear();
    layoutInfos_.reserve(bindingsCollector.size());

    for (auto& [bindings, pipelineInfo] : bindingsCollector) {
        layoutInfos_.emplace_back(LayoutInfo{bindings, std::move(pipelineInfo)});
    }
}

VkDescriptorSetLayoutBinding
ShaderManager::createLayoutBindingFromReflect(const SpvReflectDescriptorBinding* binding,
                                              VkShaderStageFlagBits shaderStage) const
{
    VkDescriptorSetLayoutBinding layoutBinding = {};
    layoutBinding.binding = binding->binding;
    layoutBinding.descriptorType = static_cast<VkDescriptorType>(binding->descriptor_type);

    // Note: SAMPLED_IMAGE -> COMBINED_IMAGE_SAMPLER conversion
    if (layoutBinding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) {
        layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    }

    layoutBinding.descriptorCount = binding->count;
    layoutBinding.stageFlags = static_cast<VkShaderStageFlags>(shaderStage);
    layoutBinding.pImmutableSamplers = nullptr;

    return layoutBinding;
}

void ShaderManager::collectPerPipelineBindings(
    const string& pipelineName,
    map<uint32_t, map<uint32_t, VkDescriptorSetLayoutBinding>>& bindingCollector) const
{
    const auto& shaders = pipelineShaders_.at(pipelineName);

    for (const auto& shader : shaders) {
        const auto& reflectModule = shader.reflectModule_;

        for (uint32_t i = 0; i < reflectModule.descriptor_binding_count; ++i) {
            const SpvReflectDescriptorBinding* binding = &reflectModule.descriptor_bindings[i];

            if (!binding->name) {
                exitWithMessage("Binding name is empty. Investigate.");
                continue; // Skip bindings without names
            }

            uint32_t setIndex = binding->set;
            uint32_t bindingIndex = binding->binding;

            auto [bindingIt, inserted] = bindingCollector[setIndex].try_emplace(bindingIndex);
            if (inserted) {
                bindingIt->second = createLayoutBindingFromReflect(binding, shader.stage_);
            } else {
                bindingIt->second.stageFlags |= static_cast<VkShaderStageFlags>(shader.stage_);
            }
        }
    }
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
    // Clean up all shaders in all pipelines
    for (auto& [pipelineName, shaders] : pipelineShaders_) {
        for (auto& shader : shaders) {
            shader.cleanup();
        }
    }

    // Clear the container after cleanup
    pipelineShaders_.clear();
}

vector<VkPipelineShaderStageCreateInfo>
ShaderManager::createPipelineShaderStageCIs(string pipelineName) const
{
    const auto& shaders = pipelineShaders_.at(pipelineName);

    vector<VkPipelineShaderStageCreateInfo> shaderStages;
    for (const auto& shader : shaders) {

        VkPipelineShaderStageCreateInfo stageCI = {};
        stageCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageCI.stage = shader.stage_; // ex: VK_SHADER_STAGE_VERTEX_BIT
        stageCI.module = shader.shaderModule_;
        stageCI.pName = shader.reflectModule_.entry_point_name; // ex: "main"
        stageCI.pSpecializationInfo = nullptr;                  // 필요하면 추가

        shaderStages.push_back(stageCI);
    }

    return shaderStages;
}

vector<VkVertexInputAttributeDescription>
ShaderManager::createVertexInputAttrDesc(string pipelineName) const
{
    for (const auto& shader : pipelineShaders_.at(pipelineName)) {
        if (shader.stage_ == VK_SHADER_STAGE_VERTEX_BIT) {
            return shader.makeVertexInputAttributeDescriptions();
        }
    }

    exitWithMessage("No vertex shader found in the shader manager.");
    return {};
}

// Add this helper function to extract member variables from SpvReflectBlockVariable
static string extractTypeName(const SpvReflectTypeDescription* typeDesc)
{
    if (!typeDesc) {
        return "unknown";
    }

    if (typeDesc->type_name) {
        return typeDesc->type_name;
    } else if (typeDesc->traits.numeric.matrix.column_count > 1) {
        return std::format("mat{}x{}", typeDesc->traits.numeric.matrix.column_count,
                           typeDesc->traits.numeric.matrix.row_count);
    } else if (typeDesc->traits.numeric.vector.component_count > 1) {
        // Determine component type
        string componentType;
        switch (typeDesc->op) {
        case SpvOpTypeFloat:
            componentType = (typeDesc->traits.numeric.scalar.width == 64) ? "d" : "";
            break;
        case SpvOpTypeInt:
            componentType = typeDesc->traits.numeric.scalar.signedness ? "i" : "u";
            break;
        default:
            componentType = "";
            break;
        }
        return std::format("{}vec{}", componentType,
                           typeDesc->traits.numeric.vector.component_count);
    } else {
        // Scalar types
        switch (typeDesc->op) {
        case SpvOpTypeFloat:
            return (typeDesc->traits.numeric.scalar.width == 64) ? "double" : "float";
        case SpvOpTypeInt:
            return typeDesc->traits.numeric.scalar.signedness ? "int" : "uint";
        case SpvOpTypeBool:
            return "bool";
        default:
            return "unknown";
        }
    }
}

void printCppStructFromBlock(const SpvReflectBlockVariable& block, const std::string& structName,
                             int indent)
{
    auto indentStr = [indent]() { return std::string(indent * 4, ' '); };

    printLog("{}struct {} {{\n", indentStr(), structName);

    for (uint32_t m = 0; m < block.member_count; ++m) {
        const SpvReflectBlockVariable& member = block.members[m];
        const SpvReflectTypeDescription* typeDesc = member.type_description;

        std::string memberType;
        std::string memberName = member.name ? member.name : "unnamed";

        // If the member is a struct, recurse
        if (typeDesc && typeDesc->op == SpvOpTypeStruct) {
            std::string nestedStructName = std::format("{}_{}", structName, memberName);
            printCppStructFromBlock(member, nestedStructName, indent + 1);
            memberType = nestedStructName;
        } else {
            // Map GLSL types to C++ types
            if (!typeDesc) {
                memberType = "unknown";
            } else if (typeDesc->type_name) {
                memberType = typeDesc->type_name;
            } else if (typeDesc->traits.numeric.matrix.column_count > 1) {
                memberType =
                    std::format("glm::mat{}x{}", typeDesc->traits.numeric.matrix.column_count,
                                typeDesc->traits.numeric.matrix.row_count);
            } else if (typeDesc->traits.numeric.vector.component_count > 1) {
                memberType =
                    std::format("glm::vec{}", typeDesc->traits.numeric.vector.component_count);
            } else {
                switch (typeDesc->op) {
                case SpvOpTypeFloat:
                    memberType = "float";
                    break;
                case SpvOpTypeInt:
                    memberType =
                        typeDesc->traits.numeric.scalar.signedness ? "int32_t" : "uint32_t";
                    break;
                case SpvOpTypeBool:
                    memberType = "bool";
                    break;
                default:
                    memberType = "scalar";
                    break;
                }
            }
        }

        // Handle arrays
        std::string arraySuffix;
        if (typeDesc && typeDesc->traits.array.dims_count > 0) {
            for (uint32_t d = 0; d < typeDesc->traits.array.dims_count; ++d) {
                arraySuffix += std::format("[{}]", typeDesc->traits.array.dims[d]);
            }
        }

        printLog("{}    {} {}{}; // offset: {}, size: {}\n", indentStr(), memberType, memberName,
                 arraySuffix, member.offset, member.size);
    }

    printLog("{}}};\n", indentStr());
}

} // namespace hlab