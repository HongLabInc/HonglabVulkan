#include "Sampler.h"
#include "Context.h"
#include "VulkanTools.h"
#include "Logger.h"

namespace hlab {

Sampler::Sampler(Context& ctx) : ctx_(ctx)
{
}

Sampler::Sampler(Sampler&& other) noexcept : ctx_(other.ctx_), sampler_(other.sampler_)
{
    other.sampler_ = VK_NULL_HANDLE;
}

Sampler::~Sampler()
{
    cleanup();
}

VkSampler Sampler::handle() const
{
    return sampler_;
}

void Sampler::createAnisoRepeat()
{
    cleanup();

    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.pNext = nullptr;
    samplerInfo.flags = 0;

    // Filtering
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    // Address modes - use REPEAT for normal texture wrapping
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    // LOD settings
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = VK_LOD_CLAMP_NONE; // Allow all mip levels

    // Check if anisotropy is supported and enabled
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(ctx_.physicalDevice(), &deviceFeatures);

    if (deviceFeatures.samplerAnisotropy) {
        // Anisotropy settings - only if supported
        samplerInfo.anisotropyEnable = VK_TRUE;

        // Get device limits for max anisotropy
        VkPhysicalDeviceProperties deviceProps;
        vkGetPhysicalDeviceProperties(ctx_.physicalDevice(), &deviceProps);
        samplerInfo.maxAnisotropy = deviceProps.limits.maxSamplerAnisotropy;
    } else {
        // Fallback to no anisotropy
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0f;
        printLog("Warning: Anisotropic filtering not supported, using linear filtering");
    }

    // Comparison settings (typically for shadow mapping)
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

    // Border color (only used with CLAMP_TO_BORDER)
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

    // Normalized coordinates
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    check(vkCreateSampler(ctx_.device(), &samplerInfo, nullptr, &sampler_));
}

void Sampler::createAnisoClamp()
{
    cleanup();

    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    // Clamped addressing
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = VK_LOD_CLAMP_NONE;

    // Enable anisotropic filtering if supported
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(ctx_.physicalDevice(), &deviceFeatures);

    if (deviceFeatures.samplerAnisotropy) {
        samplerInfo.anisotropyEnable = VK_TRUE;
        VkPhysicalDeviceProperties deviceProps;
        vkGetPhysicalDeviceProperties(ctx_.physicalDevice(), &deviceProps);
        samplerInfo.maxAnisotropy = deviceProps.limits.maxSamplerAnisotropy;
    } else {
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0f;
    }

    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    check(vkCreateSampler(ctx_.device(), &samplerInfo, nullptr, &sampler_));
}

void Sampler::createLinearRepeat()
{
    cleanup();

    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.pNext = nullptr;
    samplerInfo.flags = 0;

    // Filtering
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    // Address modes - use REPEAT for normal texture wrapping
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    // LOD settings
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = VK_LOD_CLAMP_NONE; // Allow all mip levels

    // No anisotropy for this sampler
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;

    // Comparison settings (typically for shadow mapping)
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

    // Border color (only used with CLAMP_TO_BORDER)
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

    // Normalized coordinates
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    check(vkCreateSampler(ctx_.device(), &samplerInfo, nullptr, &sampler_));
}

void Sampler::createLinearClamp()
{
    cleanup();

    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.pNext = nullptr;
    samplerInfo.flags = 0;

    // Filtering
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    // Address modes - use CLAMP_TO_EDGE to prevent wrapping
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    // LOD settings
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = VK_LOD_CLAMP_NONE; // Allow all mip levels

    // No anisotropy for this sampler
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;

    // Comparison settings (typically for shadow mapping)
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

    // Border color (only used with CLAMP_TO_BORDER)
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

    // Normalized coordinates
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    check(vkCreateSampler(ctx_.device(), &samplerInfo, nullptr, &sampler_));
}

void Sampler::createShadow()
{
    cleanup();

    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.pNext = nullptr;
    samplerInfo.flags = 0;

    // Filtering - Linear filtering for smooth shadow edges (PCF)
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

    // Address modes - Clamp to edge to prevent shadow wrapping
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    // LOD settings
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = VK_LOD_CLAMP_NONE;

    // No anisotropy for shadow maps
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;

    // Enable comparison for shadow mapping - this is the key difference
    samplerInfo.compareEnable = VK_TRUE;
    samplerInfo.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL; // For shadow comparison

    // Border color - white means fully lit (outside shadow map bounds)
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

    // Normalized coordinates
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    check(vkCreateSampler(ctx_.device(), &samplerInfo, nullptr, &sampler_));
}

void Sampler::cleanup()
{
    if (sampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(ctx_.device(), sampler_, nullptr);
        sampler_ = VK_NULL_HANDLE;
    }
}

} // namespace hlab