#include "Mesh.h"
#include "Context.h"
#include "Material.h"
#include "Vertex.h"
#include "ViewFrustum.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <glm/glm.hpp>
#include <iostream>

namespace hlab {

void Mesh::createBuffers(Context& ctx)
{
    VkDeviceSize vertexBufferSize = sizeof(vertices_[0]) * vertices_.size();
    VkDeviceSize indexBufferSize = sizeof(indices_[0]) * indices_.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = vertexBufferSize + indexBufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    check(vkCreateBuffer(ctx.device(), &bufferInfo, nullptr, &stagingBuffer));

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(ctx.device(), stagingBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = ctx.getMemoryTypeIndex(memRequirements.memoryTypeBits,
                                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    check(vkAllocateMemory(ctx.device(), &allocInfo, nullptr, &stagingBufferMemory));
    check(vkBindBufferMemory(ctx.device(), stagingBuffer, stagingBufferMemory, 0));

    void* data;
    check(vkMapMemory(ctx.device(), stagingBufferMemory, 0, vertexBufferSize + indexBufferSize, 0,
                      &data));
    memcpy(data, vertices_.data(), static_cast<size_t>(vertexBufferSize));
    memcpy(static_cast<char*>(data) + vertexBufferSize, indices_.data(),
           static_cast<size_t>(indexBufferSize));
    vkUnmapMemory(ctx.device(), stagingBufferMemory);

    bufferInfo.size = vertexBufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    check(vkCreateBuffer(ctx.device(), &bufferInfo, nullptr, &vertexBuffer_));

    vkGetBufferMemoryRequirements(ctx.device(), vertexBuffer_, &memRequirements);
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex =
        ctx.getMemoryTypeIndex(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    check(vkAllocateMemory(ctx.device(), &allocInfo, nullptr, &vertexMemory_));
    check(vkBindBufferMemory(ctx.device(), vertexBuffer_, vertexMemory_, 0));

    bufferInfo.size = indexBufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    check(vkCreateBuffer(ctx.device(), &bufferInfo, nullptr, &indexBuffer_));

    vkGetBufferMemoryRequirements(ctx.device(), indexBuffer_, &memRequirements);
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex =
        ctx.getMemoryTypeIndex(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    check(vkAllocateMemory(ctx.device(), &allocInfo, nullptr, &indexMemory_));
    check(vkBindBufferMemory(ctx.device(), indexBuffer_, indexMemory_, 0));

    CommandBuffer cmd = ctx.createGraphicsCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    VkBufferCopy copyRegion{};
    copyRegion.size = vertexBufferSize;
    vkCmdCopyBuffer(cmd.handle(), stagingBuffer, vertexBuffer_, 1, &copyRegion);

    copyRegion.srcOffset = vertexBufferSize;
    copyRegion.dstOffset = 0;
    copyRegion.size = indexBufferSize;
    vkCmdCopyBuffer(cmd.handle(), stagingBuffer, indexBuffer_, 1, &copyRegion);

    cmd.submitAndWait();

    vkDestroyBuffer(ctx.device(), stagingBuffer, nullptr);
    vkFreeMemory(ctx.device(), stagingBufferMemory, nullptr);

    calculateBounds();
}

void Mesh::calculateBounds()
{
    minBounds = vec3(FLT_MAX);
    maxBounds = vec3(-FLT_MAX);

    for (const auto& vertex : vertices_) {
        minBounds = min(minBounds, vertex.position);
        maxBounds = max(maxBounds, vertex.position);
    }
}

// Update Mesh::updateWorldBounds implementation
void Mesh::updateWorldBounds(const glm::mat4& modelMatrix)
{
    AABB localBounds(minBounds, maxBounds);
    worldBounds = localBounds.transform(modelMatrix);
}

void Mesh::cleanup(VkDevice device)
{
    if (vertexBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, vertexBuffer_, nullptr);
        vertexBuffer_ = VK_NULL_HANDLE;
    }
    if (vertexMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device, vertexMemory_, nullptr);
        vertexMemory_ = VK_NULL_HANDLE;
    }
    if (indexBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, indexBuffer_, nullptr);
        indexBuffer_ = VK_NULL_HANDLE;
    }
    if (indexMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device, indexMemory_, nullptr);
        indexMemory_ = VK_NULL_HANDLE;
    }
}

// Binary file I/O implementation
bool Mesh::writeToBinaryFileStream(std::ofstream& stream) const
{
    // File format version for future compatibility
    const uint32_t fileVersion = 1;
    if (!writeValue(stream, fileVersion))
        return false;

    // Write mesh name
    if (!writeString(stream, name_))
        return false;

    // Write material index
    if (!writeValue(stream, materialIndex_))
        return false;

    // Write vertex data
    if (!writeVector(stream, vertices_))
        return false;

    // Write index data
    if (!writeVector(stream, indices_))
        return false;

    // Write bounding box
    if (!writeValue(stream, minBounds))
        return false;
    if (!writeValue(stream, maxBounds))
        return false;

    // Write flags
    if (!writeValue(stream, isCulled))
        return false;
    if (!writeValue(stream, noTextureCoords))
        return false;

    return stream.good();
}

bool Mesh::readFromBinaryFileStream(std::ifstream& stream)
{
    // Read and verify file format version
    uint32_t fileVersion;
    if (!readValue(stream, fileVersion))
        return false;

    if (fileVersion != 1) {
        std::cerr << "Unsupported mesh file version: " << fileVersion << std::endl;
        return false;
    }

    // Read mesh name
    if (!readString(stream, name_))
        return false;

    // Read material index
    if (!readValue(stream, materialIndex_))
        return false;

    // Read vertex data
    if (!readVector(stream, vertices_))
        return false;

    // Read index data
    if (!readVector(stream, indices_))
        return false;

    // Read bounding box
    if (!readValue(stream, minBounds))
        return false;
    if (!readValue(stream, maxBounds))
        return false;

    // Read flags
    if (!readValue(stream, isCulled))
        return false;
    if (!readValue(stream, noTextureCoords))
        return false;

    // Reset Vulkan handles (they need to be recreated)
    vertexBuffer_ = VK_NULL_HANDLE;
    vertexMemory_ = VK_NULL_HANDLE;
    indexBuffer_ = VK_NULL_HANDLE;
    indexMemory_ = VK_NULL_HANDLE;

    // Initialize world bounds from local bounds
    worldBounds = AABB(minBounds, maxBounds);

    return stream.good();
}

// Helper method implementations
bool Mesh::writeString(std::ofstream& stream, const std::string& str) const
{
    uint32_t length = static_cast<uint32_t>(str.length());
    if (!writeValue(stream, length))
        return false;

    if (length > 0) {
        stream.write(str.c_str(), length);
        return stream.good();
    }
    return true;
}

bool Mesh::readString(std::ifstream& stream, std::string& str)
{
    uint32_t length;
    if (!readValue(stream, length))
        return false;

    if (length > 0) {
        str.resize(length);
        stream.read(&str[0], length);
        return stream.good();
    } else {
        str.clear();
        return true;
    }
}

} // namespace hlab
