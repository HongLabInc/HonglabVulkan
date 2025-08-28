#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <spirv-reflect/spirv_reflect.h>

namespace hlab {

using namespace std;

// forward declarations
struct DescriptorRequirement;

void check(VkResult result);

auto getResultString(VkResult errorCode) -> string;
auto descriptorTypeToString(uint32_t type) -> string;
auto getFormatSize(VkFormat format) -> uint32_t;
auto vkFormatToString(VkFormat format) -> string;
auto convertShaderStageToPS2(VkShaderStageFlags shaderStageFlags) -> VkPipelineStageFlags2;
auto getSpvReflectResultString(SpvReflectResult result) -> string;
auto getVkFormatFromSpvReflectFormat(SpvReflectFormat format) -> VkFormat;
auto shaderStageFlagsToString(VkShaderStageFlags flags) -> string;
auto descriptorTypeToString(VkDescriptorType type) -> string;
auto stringToDescriptorType(const string& typeStr) -> VkDescriptorType;
auto getVkFormatFromTypeName(const string& typeName) -> VkFormat;
auto getVkFormatSize(VkFormat format) -> uint32_t;
auto getRequiredImageLayout(VkDescriptorType type_) -> VkImageLayout;
auto getRequiredAccess(VkDescriptorType type_, bool readOnly_, bool writeOnly_) -> VkAccessFlags2;
auto colorSpaceToString(VkColorSpaceKHR colorSpace) -> const char*;

// Custom comparator for vector<VkDescriptorSetLayoutBinding>
struct BindingComp
{
    bool operator()(const vector<VkDescriptorSetLayoutBinding>& lhs,
                    const vector<VkDescriptorSetLayoutBinding>& rhs) const;
};

// Hash function for vector<VkDescriptorSetLayoutBinding>
struct BindingHash
{
    size_t operator()(const vector<VkDescriptorSetLayoutBinding>& bindings) const;
};

// Equality function for vector<VkDescriptorSetLayoutBinding>
struct BindingEqual
{
    bool operator()(const vector<VkDescriptorSetLayoutBinding>& lhs,
                    const vector<VkDescriptorSetLayoutBinding>& rhs) const;
};

} // namespace hlab