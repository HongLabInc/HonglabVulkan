#include "VulkanTools.h"
#include "Logger.h"
#include <unordered_map>

namespace hlab {

using namespace std;

void check(VkResult result)
{
    if (result != VK_SUCCESS) {
        exitWithMessage("[Error] {} {} {}", getResultString(result), __FILE__, __LINE__);
    }
}

VkDescriptorType stringToDescriptorType(const string& typeStr)
{
    static const unordered_map<string, VkDescriptorType> stringToTypeMap = {
        {"SAMPLER", VK_DESCRIPTOR_TYPE_SAMPLER},
        {"COMBINED_IMAGE_SAMPLER", VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
        {"SAMPLED_IMAGE", VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE},
        {"STORAGE_IMAGE", VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
        {"UNIFORM_TEXEL_BUFFER", VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER},
        {"STORAGE_TEXEL_BUFFER", VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER},
        {"UNIFORM_BUFFER", VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
        {"STORAGE_BUFFER", VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
        {"UNIFORM_BUFFER_DYNAMIC", VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC},
        {"STORAGE_BUFFER_DYNAMIC", VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC},
        {"INPUT_ATTACHMENT", VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT},
        {"INLINE_UNIFORM_BLOCK", VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK},
        {"ACCELERATION_STRUCTURE_KHR", VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR},
        {"ACCELERATION_STRUCTURE_NV", VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV},
        {"SAMPLE_WEIGHT_IMAGE_QCOM", VK_DESCRIPTOR_TYPE_SAMPLE_WEIGHT_IMAGE_QCOM},
        {"BLOCK_MATCH_IMAGE_QCOM", VK_DESCRIPTOR_TYPE_BLOCK_MATCH_IMAGE_QCOM},
        {"MUTABLE_EXT", VK_DESCRIPTOR_TYPE_MUTABLE_EXT}};

    auto it = stringToTypeMap.find(typeStr);
    if (it != stringToTypeMap.end()) {
        return it->second;
    }

    exitWithMessage("Invalid type {}", typeStr);
    return VK_DESCRIPTOR_TYPE_MAX_ENUM; // Invalid type
}

VkFormat getVkFormatFromTypeName(const string& typeName)
{
    if (typeName == "float") {
        return VK_FORMAT_R32_SFLOAT;
    } else if (typeName == "vec2") {
        return VK_FORMAT_R32G32_SFLOAT;
    } else if (typeName == "vec3") {
        return VK_FORMAT_R32G32B32_SFLOAT;
    } else if (typeName == "vec4") {
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    } else if (typeName == "int") {
        return VK_FORMAT_R32_SINT;
    } else if (typeName == "ivec2") {
        return VK_FORMAT_R32G32_SINT;
    } else if (typeName == "ivec3") {
        return VK_FORMAT_R32G32B32_SINT;
    } else if (typeName == "ivec4") {
        return VK_FORMAT_R32G32B32A32_SINT;
    } else if (typeName == "uint") {
        return VK_FORMAT_R32_UINT;
    } else if (typeName == "uvec2") {
        return VK_FORMAT_R32G32_UINT;
    } else if (typeName == "uvec3") {
        return VK_FORMAT_R32G32B32_UINT;
    } else if (typeName == "uvec4") {
        return VK_FORMAT_R32G32B32A32_UINT;
    } else {
        // Default fallback - could also throw an error
        return VK_FORMAT_R32_SFLOAT;
    }
}

uint32_t getVkFormatSize(VkFormat format)
{
    switch (format) {
    case VK_FORMAT_R32_SFLOAT:
    case VK_FORMAT_R32_SINT:
    case VK_FORMAT_R32_UINT:
        return 4;
    case VK_FORMAT_R32G32_SFLOAT:
    case VK_FORMAT_R32G32_SINT:
    case VK_FORMAT_R32G32_UINT:
        return 8;
    case VK_FORMAT_R32G32B32_SFLOAT:
    case VK_FORMAT_R32G32B32_SINT:
    case VK_FORMAT_R32G32B32_UINT:
        return 12;
    case VK_FORMAT_R32G32B32A32_SFLOAT:
    case VK_FORMAT_R32G32B32A32_SINT:
    case VK_FORMAT_R32G32B32A32_UINT:
        return 16;
    default:
        return 4; // Default fallback
    }
}

auto getRequiredImageLayout(VkDescriptorType type_) -> VkImageLayout
{
    switch (type_) {
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        return VK_IMAGE_LAYOUT_GENERAL;
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    default:
        exitWithMessage("Unsupported descriptor type for image layout: {}",
                        static_cast<uint32_t>(type_));
        return VK_IMAGE_LAYOUT_UNDEFINED;
    }
}

auto getRequiredAccess(VkDescriptorType type, bool readOnly, bool writeOnly) -> VkAccessFlags2
{
    VkAccessFlags2 targetAccess = VK_ACCESS_2_NONE;

    if (type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
        if (readOnly) {
            targetAccess = VK_ACCESS_2_SHADER_READ_BIT;
        } else if (writeOnly) {
            targetAccess = VK_ACCESS_2_SHADER_WRITE_BIT;
        } else {
            // Both read and write access
            targetAccess = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
        }
    } else {
        // For sampled images and combined image samplers
        targetAccess = VK_ACCESS_2_SHADER_READ_BIT;
    }

    return targetAccess;
}

const char* colorSpaceToString(VkColorSpaceKHR colorSpace)
{
    switch (colorSpace) {
    case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR:
        return "SRGB_NONLINEAR_KHR";
    case VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT:
        return "DISPLAY_P3_NONLINEAR_EXT";
    case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT:
        return "EXTENDED_SRGB_LINEAR_EXT";
    // Add other color spaces as needed
    default:
        return "UNKNOWN";
    }
}

string descriptorTypeToString(VkDescriptorType type)
{
    switch (type) {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
        return "SAMPLER";
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        return "COMBINED_IMAGE_SAMPLER";
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        return "SAMPLED_IMAGE";
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        return "STORAGE_IMAGE";
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        return "UNIFORM_TEXEL_BUFFER";
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        return "STORAGE_TEXEL_BUFFER";
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        return "UNIFORM_BUFFER";
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        return "STORAGE_BUFFER";
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        return "UNIFORM_BUFFER_DYNAMIC";
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        return "STORAGE_BUFFER_DYNAMIC";
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        return "INPUT_ATTACHMENT";
    case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK:
        return "INLINE_UNIFORM_BLOCK";
    case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
        return "ACCELERATION_STRUCTURE_KHR";
    case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV:
        return "ACCELERATION_STRUCTURE_NV";
    case VK_DESCRIPTOR_TYPE_SAMPLE_WEIGHT_IMAGE_QCOM:
        return "SAMPLE_WEIGHT_IMAGE_QCOM";
    case VK_DESCRIPTOR_TYPE_BLOCK_MATCH_IMAGE_QCOM:
        return "BLOCK_MATCH_IMAGE_QCOM";
    case VK_DESCRIPTOR_TYPE_MUTABLE_EXT:
        return "MUTABLE_EXT";
    default:
        return "UNKNOWN_DESCRIPTOR_TYPE";
    }
}

string shaderStageFlagsToString(VkShaderStageFlags flags)
{
    std::string result;
    if (flags & VK_SHADER_STAGE_VERTEX_BIT)
        result += "VERTEX|";
    if (flags & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
        result += "TESSELLATION_CONTROL|";
    if (flags & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
        result += "TESSELLATION_EVALUATION|";
    if (flags & VK_SHADER_STAGE_GEOMETRY_BIT)
        result += "GEOMETRY|";
    if (flags & VK_SHADER_STAGE_FRAGMENT_BIT)
        result += "FRAGMENT|";
    if (flags & VK_SHADER_STAGE_COMPUTE_BIT)
        result += "COMPUTE|";
    if (flags & VK_SHADER_STAGE_RAYGEN_BIT_KHR)
        result += "RAYGEN|";
    if (flags & VK_SHADER_STAGE_ANY_HIT_BIT_KHR)
        result += "ANY_HIT|";
    if (flags & VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
        result += "CLOSEST_HIT|";
    if (flags & VK_SHADER_STAGE_MISS_BIT_KHR)
        result += "MISS|";
    if (flags & VK_SHADER_STAGE_INTERSECTION_BIT_KHR)
        result += "INTERSECTION|";
    if (flags & VK_SHADER_STAGE_CALLABLE_BIT_KHR)
        result += "CALLABLE|";
    if (flags & VK_SHADER_STAGE_TASK_BIT_EXT)
        result += "TASK|";
    if (flags & VK_SHADER_STAGE_MESH_BIT_EXT)
        result += "MESH|";
    if (!result.empty())
        result.pop_back(); // Remove trailing '|'
    return result.empty() ? "NONE" : result;
}

string getResultString(VkResult errorCode)
{
    switch (errorCode) {
    case VK_SUCCESS:
        return "VK_SUCCESS";
    case VK_NOT_READY:
        return "VK_NOT_READY";
    case VK_TIMEOUT:
        return "VK_TIMEOUT";
    case VK_EVENT_SET:
        return "VK_EVENT_SET";
    case VK_EVENT_RESET:
        return "VK_EVENT_RESET";
    case VK_INCOMPLETE:
        return "VK_INCOMPLETE";
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED:
        return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST:
        return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_MEMORY_MAP_FAILED:
        return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT:
        return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT:
        return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT:
        return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER:
        return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS:
        return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FORMAT_NOT_SUPPORTED:
        return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_FRAGMENTED_POOL:
        return "VK_ERROR_FRAGMENTED_POOL";
    case VK_ERROR_UNKNOWN:
        return "VK_ERROR_UNKNOWN";
    case VK_ERROR_OUT_OF_POOL_MEMORY:
        return "VK_ERROR_OUT_OF_POOL_MEMORY";
    case VK_ERROR_INVALID_EXTERNAL_HANDLE:
        return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
    case VK_ERROR_FRAGMENTATION:
        return "VK_ERROR_FRAGMENTATION";
    case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
        return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
    case VK_PIPELINE_COMPILE_REQUIRED:
        return "VK_PIPELINE_COMPILE_REQUIRED";
    case VK_ERROR_SURFACE_LOST_KHR:
        return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
        return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
    case VK_SUBOPTIMAL_KHR:
        return "VK_SUBOPTIMAL_KHR";
    case VK_ERROR_OUT_OF_DATE_KHR:
        return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
        return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
    case VK_ERROR_VALIDATION_FAILED_EXT:
        return "VK_ERROR_VALIDATION_FAILED_EXT";
    case VK_ERROR_INVALID_SHADER_NV:
        return "VK_ERROR_INVALID_SHADER_NV";
    case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR:
        return "VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR:
        return "VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR:
        return "VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR:
        return "VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR:
        return "VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR:
        return "VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR";
    case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT:
        return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
    case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
        return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
    case VK_THREAD_IDLE_KHR:
        return "VK_THREAD_IDLE_KHR";
    case VK_THREAD_DONE_KHR:
        return "VK_THREAD_DONE_KHR";
    case VK_OPERATION_DEFERRED_KHR:
        return "VK_OPERATION_DEFERRED_KHR";
    case VK_OPERATION_NOT_DEFERRED_KHR:
        return "VK_OPERATION_NOT_DEFERRED_KHR";
    case VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR:
        return "VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR";
    case VK_ERROR_COMPRESSION_EXHAUSTED_EXT:
        return "VK_ERROR_COMPRESSION_EXHAUSTED_EXT";
    case VK_RESULT_MAX_ENUM:
        return "VK_RESULT_MAX_ENUM";
    default:
        return "UNKNOWN_ERROR";
    }
}

string descriptorTypeToString(uint32_t type)
{
    switch (type) {
    case 0:
        return "SAMPLER";
    case 1:
        return "COMBINED_IMAGE_SAMPLER";
    case 2:
        return "SAMPLED_IMAGE";
    case 3:
        return "STORAGE_IMAGE";
    case 4:
        return "UNIFORM_TEXEL_BUFFER";
    case 5:
        return "STORAGE_TEXEL_BUFFER";
    case 6:
        return "UNIFORM_BUFFER";
    case 7:
        return "STORAGE_BUFFER";
    case 8:
        return "UNIFORM_BUFFER_DYNAMIC";
    case 9:
        return "STORAGE_BUFFER_DYNAMIC";
    case 10:
        return "INPUT_ATTACHMENT";
    case 1000138000:
        return "INLINE_UNIFORM_BLOCK";
    case 1000150000:
        return "ACCELERATION_STRUCTURE_KHR";
    case 1000165000:
        return "ACCELERATION_STRUCTURE_NV";
    case 1000440000:
        return "SAMPLE_WEIGHT_IMAGE_QCOM";
    case 1000440001:
        return "BLOCK_MATCH_IMAGE_QCOM";
    case 1000351000:
        return "MUTABLE_EXT";
    case 1000570000:
        return "PARTITIONED_ACCELERATION_STRUCTURE_NV";
    default:
        return "UNKNOWN_DESCRIPTOR_TYPE";
    }
}

uint32_t getFormatSize(VkFormat format)
{
    switch (format) {
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8_UINT:
    case VK_FORMAT_R8_SINT:
        return 1;
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R8G8_UINT:
    case VK_FORMAT_R8G8_SINT:
        return 2;
    case VK_FORMAT_R8G8B8_UNORM:
    case VK_FORMAT_R8G8B8_UINT:
    case VK_FORMAT_R8G8B8_SINT:
        return 3;
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_UINT:
    case VK_FORMAT_R8G8B8A8_SINT:
    case VK_FORMAT_R32_SFLOAT:
        return 4;
    case VK_FORMAT_R32G32_SFLOAT:
        return 8;
    case VK_FORMAT_R32G32B32_SFLOAT:
        return 12;
    case VK_FORMAT_R32G32B32A32_SFLOAT:
        return 16;
    case VK_FORMAT_R32_SINT:
        return 4;
    case VK_FORMAT_R32G32_SINT:
        return 8;
    case VK_FORMAT_R32G32B32_SINT:
        return 12;
    case VK_FORMAT_R32G32B32A32_SINT:
        return 16;
    case VK_FORMAT_R32_UINT:
        return 4;
    case VK_FORMAT_R32G32_UINT:
        return 8;
    case VK_FORMAT_R32G32B32_UINT:
        return 12;
    case VK_FORMAT_R32G32B32A32_UINT:
        return 16;
    default:
        exitWithMessage("Unsupported format.");
        return 0; // Unknown/unsupported format
    }
}

std::string vkFormatToString(VkFormat format)
{
    switch (format) {
    case VK_FORMAT_UNDEFINED:
        return "UNDEFINED";
    case VK_FORMAT_R4G4_UNORM_PACK8:
        return "R4G4_UNORM_PACK8";
    case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
        return "R4G4B4A4_UNORM_PACK16";
    case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
        return "B4G4R4A4_UNORM_PACK16";
    case VK_FORMAT_R5G6B5_UNORM_PACK16:
        return "R5G6B5_UNORM_PACK16";
    case VK_FORMAT_B5G6R5_UNORM_PACK16:
        return "B5G6R5_UNORM_PACK16";
    case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
        return "R5G5B5A1_UNORM_PACK16";
    case VK_FORMAT_B5G5R5A1_UNORM_PACK16:
        return "B5G5R5A1_UNORM_PACK16";
    case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
        return "A1R5G5B5_UNORM_PACK16";
    case VK_FORMAT_R8_UNORM:
        return "R8_UNORM";
    case VK_FORMAT_R8_SNORM:
        return "R8_SNORM";
    case VK_FORMAT_R8_UINT:
        return "R8_UINT";
    case VK_FORMAT_R8_SINT:
        return "R8_SINT";
    case VK_FORMAT_R8_SRGB:
        return "R8_SRGB";
    case VK_FORMAT_R8G8_UNORM:
        return "R8G8_UNORM";
    case VK_FORMAT_R8G8_SNORM:
        return "R8G8_SNORM";
    case VK_FORMAT_R8G8_UINT:
        return "R8G8_UINT";
    case VK_FORMAT_R8G8_SINT:
        return "R8G8_SINT";
    case VK_FORMAT_R8G8_SRGB:
        return "R8G8_SRGB";
    case VK_FORMAT_R8G8B8_UNORM:
        return "R8G8B8_UNORM";
    case VK_FORMAT_R8G8B8_SNORM:
        return "R8G8B8_SNORM";
    case VK_FORMAT_R8G8B8_UINT:
        return "R8G8B8_UINT";
    case VK_FORMAT_R8G8B8_SINT:
        return "R8G8B8_SINT";
    case VK_FORMAT_R8G8B8_SRGB:
        return "R8G8B8_SRGB";
    case VK_FORMAT_B8G8R8_UNORM:
        return "B8G8R8_UNORM";
    case VK_FORMAT_B8G8R8_SNORM:
        return "B8G8R8_SNORM";
    case VK_FORMAT_B8G8R8_UINT:
        return "B8G8R8_UINT";
    case VK_FORMAT_B8G8R8_SINT:
        return "B8G8R8_SINT";
    case VK_FORMAT_B8G8R8_SRGB:
        return "B8G8R8_SRGB";
    case VK_FORMAT_R8G8B8A8_UNORM:
        return "R8G8B8A8_UNORM";
    case VK_FORMAT_R8G8B8A8_SNORM:
        return "R8G8B8A8_SNORM";
    case VK_FORMAT_R8G8B8A8_UINT:
        return "R8G8B8A8_UINT";
    case VK_FORMAT_R8G8B8A8_SINT:
        return "R8G8B8A8_SINT";
    case VK_FORMAT_R8G8B8A8_SRGB:
        return "R8G8B8A8_SRGB";
    case VK_FORMAT_B8G8R8A8_UNORM:
        return "B8G8R8A8_UNORM";
    case VK_FORMAT_B8G8R8A8_SNORM:
        return "B8G8R8A8_SNORM";
    case VK_FORMAT_B8G8R8A8_UINT:
        return "B8G8R8A8_UINT";
    case VK_FORMAT_B8G8R8A8_SINT:
        return "B8G8R8A8_SINT";
    case VK_FORMAT_B8G8R8A8_SRGB:
        return "B8G8R8A8_SRGB";
    case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
        return "A8B8G8R8_UNORM_PACK32";
    case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
        return "A8B8G8R8_SNORM_PACK32";
    case VK_FORMAT_A8B8G8R8_UINT_PACK32:
        return "A8B8G8R8_UINT_PACK32";
    case VK_FORMAT_A8B8G8R8_SINT_PACK32:
        return "A8B8G8R8_SINT_PACK32";
    case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
        return "A8B8G8R8_SRGB_PACK32";
    case VK_FORMAT_D16_UNORM:
        return "D16_UNORM";
    case VK_FORMAT_X8_D24_UNORM_PACK32:
        return "X8_D24_UNORM_PACK32";
    case VK_FORMAT_D32_SFLOAT:
        return "D32_SFLOAT";
    case VK_FORMAT_S8_UINT:
        return "S8_UINT";
    case VK_FORMAT_D16_UNORM_S8_UINT:
        return "D16_UNORM_S8_UINT";
    case VK_FORMAT_D24_UNORM_S8_UINT:
        return "D24_UNORM_S8_UINT";
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return "D32_SFLOAT_S8_UINT";
    default:
        return "UNKNOWN_FORMAT(" + std::to_string(static_cast<int>(format)) + ")";
    }
}

VkPipelineStageFlags2 convertShaderStageToPS2(VkShaderStageFlags shaderStageFlags)
{
    VkPipelineStageFlags2 stageFlags = VK_PIPELINE_STAGE_2_NONE;

    static const std::pair<VkShaderStageFlags, VkPipelineStageFlags2> stageMap[] = {
        {VK_SHADER_STAGE_VERTEX_BIT, VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT},
        {VK_SHADER_STAGE_FRAGMENT_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT},
        {VK_SHADER_STAGE_COMPUTE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT},
        {VK_SHADER_STAGE_GEOMETRY_BIT, VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT},
        {VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
         VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT},
        {VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
         VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT}};

    for (const auto& [shaderStage, pipelineStage] : stageMap) {
        if (shaderStageFlags & shaderStage) {
            stageFlags |= pipelineStage;
        }
    }

    return stageFlags != VK_PIPELINE_STAGE_2_NONE ? stageFlags
                                                  : VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
}

string getSpvReflectResultString(SpvReflectResult result)
{
    switch (result) {
    case SPV_REFLECT_RESULT_SUCCESS:
        return "SPV_REFLECT_RESULT_SUCCESS";
    case SPV_REFLECT_RESULT_NOT_READY:
        return "SPV_REFLECT_RESULT_NOT_READY";
    case SPV_REFLECT_RESULT_ERROR_PARSE_FAILED:
        return "SPV_REFLECT_RESULT_ERROR_PARSE_FAILED";
    case SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED:
        return "SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED";
    case SPV_REFLECT_RESULT_ERROR_RANGE_EXCEEDED:
        return "SPV_REFLECT_RESULT_ERROR_RANGE_EXCEEDED";
    case SPV_REFLECT_RESULT_ERROR_NULL_POINTER:
        return "SPV_REFLECT_RESULT_ERROR_NULL_POINTER";
    case SPV_REFLECT_RESULT_ERROR_INTERNAL_ERROR:
        return "SPV_REFLECT_RESULT_ERROR_INTERNAL_ERROR";
    case SPV_REFLECT_RESULT_ERROR_COUNT_MISMATCH:
        return "SPV_REFLECT_RESULT_ERROR_COUNT_MISMATCH";
    case SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND:
        return "SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND";
    case SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_CODE_SIZE:
        return "SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_CODE_SIZE";
    case SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_MAGIC_NUMBER:
        return "SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_MAGIC_NUMBER";
    case SPV_REFLECT_RESULT_ERROR_SPIRV_UNEXPECTED_EOF:
        return "SPV_REFLECT_RESULT_ERROR_SPIRV_UNEXPECTED_EOF";
    case SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE:
        return "SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE";
    case SPV_REFLECT_RESULT_ERROR_SPIRV_SET_NUMBER_OVERFLOW:
        return "SPV_REFLECT_RESULT_ERROR_SPIRV_SET_NUMBER_OVERFLOW";
    case SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_STORAGE_CLASS:
        return "SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_STORAGE_CLASS";
    default:
        return "SPV_REFLECT_RESULT_UNKNOWN";
    }
}

VkFormat getVkFormatFromSpvReflectFormat(SpvReflectFormat format)
{
    switch (format) {
    case SPV_REFLECT_FORMAT_R32G32B32_SFLOAT:
        return VK_FORMAT_R32G32B32_SFLOAT;
    case SPV_REFLECT_FORMAT_R32G32_SFLOAT:
        return VK_FORMAT_R32G32_SFLOAT;
    case SPV_REFLECT_FORMAT_R32_SFLOAT:
        return VK_FORMAT_R32_SFLOAT;
    default:
        exitWithMessage("Unsupported SPIR-V format");
        return VK_FORMAT_UNDEFINED; // Unsupported or unknown format
    }
}

bool BindingComp::operator()(const vector<VkDescriptorSetLayoutBinding>& lhs,
                             const vector<VkDescriptorSetLayoutBinding>& rhs) const
{
    // First compare sizes
    if (lhs.size() != rhs.size()) {
        return lhs.size() < rhs.size();
    }

    // Compare each binding element by element
    for (size_t i = 0; i < lhs.size(); ++i) {
        const auto& l = lhs[i];
        const auto& r = rhs[i];

        // Compare each field in order
        if (l.binding != r.binding) {
            return l.binding < r.binding;
        }
        if (l.descriptorType != r.descriptorType) {
            return l.descriptorType < r.descriptorType;
        }
        if (l.descriptorCount != r.descriptorCount) {
            return l.descriptorCount < r.descriptorCount;
        }
        if (l.stageFlags != r.stageFlags) {
            return l.stageFlags < r.stageFlags;
        }
        // Compare pImmutableSamplers pointer values
        // Note: This compares pointer addresses, not content
        if (l.pImmutableSamplers != r.pImmutableSamplers) {
            return l.pImmutableSamplers < r.pImmutableSamplers;
        }
    }

    // All elements are equal
    return false;
}

size_t BindingHash::operator()(const vector<VkDescriptorSetLayoutBinding>& bindings) const
{
    size_t hash = 0;
    for (const auto& binding : bindings) {
        // Combine hash values for each field
        hash ^= std::hash<uint32_t>{}(binding.binding) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^=
            std::hash<uint32_t>{}(binding.descriptorType) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^=
            std::hash<uint32_t>{}(binding.descriptorCount) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<const void*>{}(binding.pImmutableSamplers) + 0x9e3779b9 + (hash << 6) +
                (hash >> 2);
        // Note: stageFlags is intentionally removed from hash generation.
        // hash ^=
        //     std::hash<uint32_t>{}(binding.stageFlags) + 0x9e3779b9 + (hash << 6) + (hash >>
        //     2);
    }
    return hash;
}

bool BindingEqual::operator()(const vector<VkDescriptorSetLayoutBinding>& lhs,
                              const vector<VkDescriptorSetLayoutBinding>& rhs) const
{
    if (lhs.size() != rhs.size()) {
        return false;
    }

    for (size_t i = 0; i < lhs.size(); ++i) {
        const auto& l = lhs[i];
        const auto& r = rhs[i];

        // Note: stageFlags is not used to determine equality intentionally.

        if (l.binding != r.binding || l.descriptorType != r.descriptorType ||
            l.descriptorCount != r.descriptorCount ||
            /*  l.stageFlags != r.stageFlags || */
            l.pImmutableSamplers != r.pImmutableSamplers) {
            return false;
        }
    }
    return true;
}

} // namespace hlab