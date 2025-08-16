#include "Material.h"
#include "Context.h"
#include "Sampler.h"
#include "Image2D.h"
#include "VulkanTools.h"

#include <fstream>

namespace hlab {

// void Material::createUniformBuffer(Context& ctx)
//{
//     // Use MaterialUBO size instead of Material size for proper alignment
//     VkDeviceSize bufferSize = sizeof(MaterialUBO);
//
//     VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
//     bufferInfo.size = bufferSize;
//     bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
//     bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
//
//     check(vkCreateBuffer(ctx.device(), &bufferInfo, nullptr, &uniformBuffer_));
//
//     VkMemoryRequirements memReqs;
//     vkGetBufferMemoryRequirements(ctx.device(), uniformBuffer_, &memReqs);
//
//     VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
//     allocInfo.allocationSize = memReqs.size;
//     allocInfo.memoryTypeIndex =
//         ctx.getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
//                                                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
//
//     check(vkAllocateMemory(ctx.device(), &allocInfo, nullptr, &uniformMemory_));
//     check(vkBindBufferMemory(ctx.device(), uniformBuffer_, uniformMemory_, 0));
//     check(vkMapMemory(ctx.device(), uniformMemory_, 0, bufferSize, 0, &uniformMapped_));
// }

// void Material::updateUniformBuffer()
//{
//     if (uniformMapped_) {
//         // Create properly aligned MaterialUBO struct
//         MaterialUBO ubo;
//
//         // Copy CPU-side data to GPU-aligned structure
//         // vec3 -> vec4 conversion (last component unused)
//         ubo.emissiveFactor_ = emissiveFactor_;
//         ubo.baseColorFactor_ = baseColorFactor_;
//         ubo.roughness_ = roughness_;
//         ubo.transparencyFactor_ = transparencyFactor_;
//         ubo.discardAlpha_ = discardAlpha_;
//         ubo.metallicFactor_ = metallicFactor_;
//         ubo.baseColorTextureIndex_ = baseColorTextureIndex_;
//         ubo.emissiveTextureIndex_ = emissiveTextureIndex_;
//         ubo.normalTextureIndex_ = normalTextureIndex_;
//         ubo.opacityTextureIndex_ = opacityTextureIndex_;
//
//         uniformBufferSize_ = sizeof(MaterialUBO); // TODO: unnecessary
//
//         memcpy(uniformMapped_, &ubo, uniformBufferSize_);
//     }
// }

// void Material::cleanup(VkDevice device)
//{
//     if (uniformBuffer_ != VK_NULL_HANDLE) {
//         vkDestroyBuffer(device, uniformBuffer_, nullptr);
//         uniformBuffer_ = VK_NULL_HANDLE;
//     }
//     if (uniformMemory_ != VK_NULL_HANDLE) {
//         vkFreeMemory(device, uniformMemory_, nullptr);
//         uniformMemory_ = VK_NULL_HANDLE;
//     }
//     uniformMapped_ = nullptr;
// }

void Material::loadFromCache(const string& cachePath)
{
    std::ifstream stream(cachePath, std::ios::binary);
    if (!stream.is_open()) {
        // Cache file doesn't exist or cannot be opened
        return;
    }

    try {
        // Read file format version for future compatibility
        uint32_t fileVersion;
        stream.read(reinterpret_cast<char*>(&fileVersion), sizeof(fileVersion));
        if (!stream.good() || fileVersion != 1) {
            return; // Unsupported version or read error
        }

        // Read material name
        uint32_t nameLength;
        stream.read(reinterpret_cast<char*>(&nameLength), sizeof(nameLength));
        if (!stream.good())
            return;

        if (nameLength > 0) {
            name_.resize(nameLength);
            stream.read(&name_[0], nameLength);
            if (!stream.good())
                return;
        }

        // Read material properties
        stream.read(reinterpret_cast<char*>(&ubo_.emissiveFactor_), sizeof(ubo_.emissiveFactor_));
        stream.read(reinterpret_cast<char*>(&ubo_.baseColorFactor_), sizeof(ubo_.baseColorFactor_));
        stream.read(reinterpret_cast<char*>(&ubo_.roughness_), sizeof(ubo_.roughness_));
        stream.read(reinterpret_cast<char*>(&ubo_.transparencyFactor_),
                    sizeof(ubo_.transparencyFactor_));
        stream.read(reinterpret_cast<char*>(&ubo_.discardAlpha_), sizeof(ubo_.discardAlpha_));
        stream.read(reinterpret_cast<char*>(&ubo_.metallicFactor_), sizeof(ubo_.metallicFactor_));

        // Read texture indices
        stream.read(reinterpret_cast<char*>(&ubo_.baseColorTextureIndex_),
                    sizeof(ubo_.baseColorTextureIndex_));
        stream.read(reinterpret_cast<char*>(&ubo_.emissiveTextureIndex_),
                    sizeof(ubo_.emissiveTextureIndex_));
        stream.read(reinterpret_cast<char*>(&ubo_.normalTextureIndex_),
                    sizeof(ubo_.normalTextureIndex_));
        stream.read(reinterpret_cast<char*>(&ubo_.opacityTextureIndex_),
                    sizeof(ubo_.opacityTextureIndex_));
        stream.read(reinterpret_cast<char*>(&ubo_.metallicRoughnessTextureIndex_),
                    sizeof(ubo_.metallicRoughnessTextureIndex_));
        stream.read(reinterpret_cast<char*>(&ubo_.occlusionTextureIndex_),
                    sizeof(ubo_.occlusionTextureIndex_));

        // Read flags
        stream.read(reinterpret_cast<char*>(&flags_), sizeof(flags_));

    } catch (...) {
        // If any error occurs, silently ignore and continue with default values
    }
}

void Material::writeToCache(const string& cachePath)
{
    std::ofstream stream(cachePath, std::ios::binary);
    if (!stream.is_open()) {
        return; // Cannot create cache file
    }

    try {
        // Write file format version for future compatibility
        const uint32_t fileVersion = 1;
        stream.write(reinterpret_cast<const char*>(&fileVersion), sizeof(fileVersion));

        // Write material name
        uint32_t nameLength = static_cast<uint32_t>(name_.length());
        stream.write(reinterpret_cast<const char*>(&nameLength), sizeof(nameLength));
        if (nameLength > 0) {
            stream.write(name_.c_str(), nameLength);
        }

        // Write material properties
        stream.write(reinterpret_cast<const char*>(&ubo_.emissiveFactor_),
                     sizeof(ubo_.emissiveFactor_));
        stream.write(reinterpret_cast<const char*>(&ubo_.baseColorFactor_),
                     sizeof(ubo_.baseColorFactor_));
        stream.write(reinterpret_cast<const char*>(&ubo_.roughness_), sizeof(ubo_.roughness_));
        stream.write(reinterpret_cast<const char*>(&ubo_.transparencyFactor_),
                     sizeof(ubo_.transparencyFactor_));
        stream.write(reinterpret_cast<const char*>(&ubo_.discardAlpha_),
                     sizeof(ubo_.discardAlpha_));
        stream.write(reinterpret_cast<const char*>(&ubo_.metallicFactor_),
                     sizeof(ubo_.metallicFactor_));

        // Write texture indices
        stream.write(reinterpret_cast<const char*>(&ubo_.baseColorTextureIndex_),
                     sizeof(ubo_.baseColorTextureIndex_));
        stream.write(reinterpret_cast<const char*>(&ubo_.emissiveTextureIndex_),
                     sizeof(ubo_.emissiveTextureIndex_));
        stream.write(reinterpret_cast<const char*>(&ubo_.normalTextureIndex_),
                     sizeof(ubo_.normalTextureIndex_));
        stream.write(reinterpret_cast<const char*>(&ubo_.opacityTextureIndex_),
                     sizeof(ubo_.opacityTextureIndex_));
        stream.write(reinterpret_cast<const char*>(&ubo_.metallicRoughnessTextureIndex_),
                     sizeof(ubo_.metallicRoughnessTextureIndex_));
        stream.write(reinterpret_cast<const char*>(&ubo_.occlusionTextureIndex_),
                     sizeof(ubo_.occlusionTextureIndex_));

        // Write flags
        stream.write(reinterpret_cast<const char*>(&flags_), sizeof(flags_));

    } catch (...) {
        // If any error occurs, silently ignore
    }
}

} // namespace hlab
