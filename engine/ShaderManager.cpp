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
    // Clear and populate bindingInfos_
    bindingInfos_.clear();

    // Use unordered_map with custom hash and equality functors
    unordered_map<vector<VkDescriptorSetLayoutBinding>, vector<tuple<string, uint32_t>>,
                  BindingHash, BindingEqual>
        bindingsCollector;

    for (const auto& [pipelineName, shaders] : pipelineShaders_) {
        map<uint32_t, map<uint32_t, VkDescriptorSetLayoutBinding>> perPipelineBindings;
        collectPerPipelineBindings(pipelineName, perPipelineBindings);

        // Collect BindingInfo for this pipeline
        vector<vector<BindingInfo>> pipelineBindingInfos;

        for (const auto& [setIndex, bindingsMap] : perPipelineBindings) {
            if (bindingsMap.empty())
                continue;

            // Ensure we have enough sets in the pipeline binding infos
            while (pipelineBindingInfos.size() <= setIndex) {
                pipelineBindingInfos.emplace_back();
            }

            vector<BindingInfo>& setBindingInfos = pipelineBindingInfos[setIndex];

            // Process each binding in the set
            for (const auto& [bindingIndex, layoutBinding] : bindingsMap) {
                // Ensure we have enough bindings in the set
                while (setBindingInfos.size() <= bindingIndex) {
                    setBindingInfos.emplace_back();
                }

                // Extract binding info from shader reflection
                BindingInfo& bindingInfo = setBindingInfos[bindingIndex];

                // Find the binding name and determine writability from shader reflection
                for (const auto& shader : shaders) {
                    const auto& reflectModule = shader.reflectModule_;

                    for (uint32_t i = 0; i < reflectModule.descriptor_binding_count; ++i) {
                        const SpvReflectDescriptorBinding* binding =
                            &reflectModule.descriptor_bindings[i];

                        if (binding->set == setIndex && binding->binding == bindingIndex) {
                            if (binding->name) {
                                bindingInfo.resourceName = string(binding->name);
                            }

                            // Set the set and binding indices
                            bindingInfo.setIndex = setIndex;
                            bindingInfo.bindingIndex = bindingIndex;

                            if (string(binding->name) == "floatColor1" ||
                                string(binding->name) == "floatColor2") {
                                cout << binding->name << endl;
                            }

                            // Determine if write-only based on resource_type and decorations
                            bool writeOnly = false;

                            // Use resource_type as primary indicator
                            if (binding->resource_type & SPV_REFLECT_RESOURCE_FLAG_UAV) {
                                // UAV (Unordered Access View) can be read-write or write-only
                                // Check decoration flags to determine exact access pattern
                                bool hasNonWritableDecoration =
                                    (binding->decoration_flags &
                                     SPV_REFLECT_DECORATION_NON_WRITABLE) != 0;
                                bool hasNonReadableDecoration =
                                    (binding->decoration_flags &
                                     SPV_REFLECT_DECORATION_NON_READABLE) != 0;

                                // Debug: Log resource type and decoration flags
                                // printLog("      Debug: '{}' resource_type={},
                                // decoration_flags={}, "
                                //         "nonWritable={}, nonReadable={}",
                                //         binding->name ? binding->name : "unnamed",
                                //         static_cast<uint32_t>(binding->resource_type),
                                //         static_cast<uint32_t>(binding->decoration_flags),
                                //         hasNonWritableDecoration, hasNonReadableDecoration);

                                // Only mark as write-only if it has NON_READABLE decoration and
                                // does NOT have NON_WRITABLE decoration
                                if (hasNonReadableDecoration && !hasNonWritableDecoration) {
                                    writeOnly = true;
                                }
                            } else {
                                // SRV, CBV, and SAMPLER are read-only by nature
                                writeOnly = false;

                                // Debug: Log non-UAV resources
                                // printLog(
                                //    "      Debug: '{}' resource_type={} (read-only resource
                                //    type)", binding->name ? binding->name : "unnamed",
                                //    static_cast<uint32_t>(binding->resource_type));
                            }

                            bindingInfo.writeonly = writeOnly;

                            // Determine target layout, access, and stage based on descriptor type and usage
                            VkDescriptorType descriptorType = static_cast<VkDescriptorType>(binding->descriptor_type);
                            VkShaderStageFlags stageFlags = static_cast<VkShaderStageFlags>(shader.stage_);

                            // Determine target layout
                            switch (descriptorType) {
                                case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                                    bindingInfo.targetLayout = VK_IMAGE_LAYOUT_GENERAL;
                                    break;
                                case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                                case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                                    bindingInfo.targetLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                                    break;
                                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                                    bindingInfo.targetLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Not applicable for buffers
                                    break;
                                default:
                                    bindingInfo.targetLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                                    break;
                            }

                            // Determine target access flags
                            switch (descriptorType) {
                                case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                                    if (writeOnly) {
                                        bindingInfo.targetAccess = VK_ACCESS_2_SHADER_WRITE_BIT;
                                    } else {
                                        bindingInfo.targetAccess = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
                                    }
                                    break;
                                case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                                case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                                    bindingInfo.targetAccess = VK_ACCESS_2_SHADER_READ_BIT;
                                    break;
                                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                                    if (writeOnly) {
                                        bindingInfo.targetAccess = VK_ACCESS_2_SHADER_WRITE_BIT;
                                    } else {
                                        bindingInfo.targetAccess = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
                                    }
                                    break;
                                default:
                                    bindingInfo.targetAccess = VK_ACCESS_2_SHADER_READ_BIT;
                                    break;
                            }

                            // Determine target pipeline stage
                            switch (shader.stage_) {
                                case VK_SHADER_STAGE_VERTEX_BIT:
                                    bindingInfo.targetStage = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
                                    break;
                                case VK_SHADER_STAGE_FRAGMENT_BIT:
                                    bindingInfo.targetStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
                                    break;
                                case VK_SHADER_STAGE_COMPUTE_BIT:
                                    bindingInfo.targetStage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
                                    break;
                                case VK_SHADER_STAGE_GEOMETRY_BIT:
                                    bindingInfo.targetStage = VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT;
                                    break;
                                case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
                                    bindingInfo.targetStage = VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT;
                                    break;
                                case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
                                    bindingInfo.targetStage = VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT;
                                    break;
                                default:
                                    bindingInfo.targetStage = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
                                    break;
                            }

                            break;
                        }
                    }
                }
            }

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

        // Store the collected binding infos for this pipeline
        bindingInfos_[pipelineName] = move(pipelineBindingInfos);
    }

    // Convert bindingsCollector to layoutInfos_
    layoutInfos_.clear();
    layoutInfos_.reserve(bindingsCollector.size());

    for (auto& [bindings, pipelineInfo] : bindingsCollector) {
        layoutInfos_.emplace_back(LayoutInfo{bindings, move(pipelineInfo)});
    }

    // Log bindingInfos_ for debugging
    printLog("\n=== Shader Manager Binding Information ===");
    for (const auto& [pipelineName, pipelineBindingInfos] : bindingInfos_) {
        printLog("Pipeline '{}': {} sets", pipelineName, pipelineBindingInfos.size());

        for (size_t setIdx = 0; setIdx < pipelineBindingInfos.size(); ++setIdx) {
            const auto& setBindings = pipelineBindingInfos[setIdx];
            if (!setBindings.empty()) {
                printLog("  Set {}: {} bindings", setIdx, setBindings.size());

                for (size_t bindingIdx = 0; bindingIdx < setBindings.size(); ++bindingIdx) {
                    const auto& bindingInfo = setBindings[bindingIdx];
                    if (!bindingInfo.resourceName.empty()) {
                        printLog("    Binding {}: name='{}', set={}, binding={}, writeonly={}, layout={}, access={}, stage={}", 
                                 bindingIdx,
                                 bindingInfo.resourceName,
                                 bindingInfo.setIndex,
                                 bindingInfo.bindingIndex,
                                 bindingInfo.writeonly ? "true" : "false",
                                 imageLayoutToString(bindingInfo.targetLayout),
                                 accessFlags2ToString(bindingInfo.targetAccess),
                                 pipelineStageFlags2ToString(bindingInfo.targetStage));
                    }
                }
            }
        }
    }
    printLog("==========================================\n");
}

VkDescriptorSetLayoutBinding
ShaderManager::createLayoutBindingFromReflect(const SpvReflectDescriptorBinding* binding,
                                              VkShaderStageFlagBits shaderStage) const
{
    VkDescriptorSetLayoutBinding layoutBinding = {};
    layoutBinding.binding = binding->binding;
    layoutBinding.descriptorType = static_cast<VkDescriptorType>(binding->descriptor_type);
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

VkPushConstantRange ShaderManager::pushConstantsRange(string pipelineName)
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

array<uint32_t, 3> ShaderManager::getComputeLocalWorkgroupSize(string pipelineName) const
{
    // Initialize with default values
    array<uint32_t, 3> workgroupSize = {1, 1, 1};

    auto it = pipelineShaders_.find(pipelineName);
    if (it == pipelineShaders_.end()) {
        printLog("[Warning] Pipeline '{}' not found in ShaderManager", pipelineName);
        return workgroupSize;
    }

    const auto& shaders = it->second;

    // Find the compute shader in this pipeline
    for (const auto& shader : shaders) {
        if (shader.stage_ == VK_SHADER_STAGE_COMPUTE_BIT) {
            return shader.getLocalWorkgroupSize();
        }
    }

    printLog("[Warning] No compute shader found in pipeline '{}'", pipelineName);
    return workgroupSize;
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