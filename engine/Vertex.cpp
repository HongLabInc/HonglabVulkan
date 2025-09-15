#include "Vertex.h"

#include <vector>
#include <algorithm>

namespace hlab {

using namespace std;

// Vertex implementation (f16 optimized format)
void Vertex::addBoneData(uint32_t boneIndex, float weight)
{
    // Find an empty slot (boneIndex == -1) to add the bone data
    for (int i = 0; i < 4; ++i) {
        if (boneIndices[i] == -1) {
            boneIndices[i] = static_cast<int>(boneIndex);
            boneWeights[i] = weight;
            return;
        }
    }

    // If no empty slot found, replace the bone with the smallest weight
    // if the new weight is larger
    int minIndex = 0;
    for (int i = 1; i < 4; ++i) {
        if (boneWeights[i] < boneWeights[minIndex]) {
            minIndex = i;
        }
    }

    if (weight > boneWeights[minIndex]) {
        boneIndices[minIndex] = static_cast<int>(boneIndex);
        boneWeights[minIndex] = weight;
    }
}

void Vertex::normalizeBoneWeights()
{
    float totalWeight = boneWeights.x + boneWeights.y + boneWeights.z + boneWeights.w;

    if (totalWeight > 0.0f) {
        boneWeights /= totalWeight;
    } else {
        // If no bone weights, set default (no animation)
        boneWeights = vec4(0.0f);
        boneIndices = ivec4(-1);
    }
}

bool Vertex::hasValidBoneData() const
{
    // Check if any bone index is valid (>= 0)
    return (boneIndices.x >= 0 || boneIndices.y >= 0 || boneIndices.z >= 0 || boneIndices.w >= 0);
}

vector<VkVertexInputAttributeDescription> Vertex::getAttributeDescriptions()
{
    // Return the full attribute descriptions including bone data
    return getAttributeDescriptionsAnimated();
}

vector<VkVertexInputAttributeDescription> Vertex::getAttributeDescriptionsBasic()
{
    /*
     * BASIC VERTEX INPUT ATTRIBUTE DESCRIPTIONS (WITHOUT BONE DATA):
     *
     * Uses half-precision formats for geometric attributes to reduce memory bandwidth.
     * This is ideal for static models that don't use skeletal animation.
     */

    vector<VkVertexInputAttributeDescription> attributeDescriptions(5);

    // Position attribute (location = 0) - half-precision
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R16G16B16_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, position);

    // Normal attribute (location = 1) - half-precision
    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R16G16B16_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, normal);

    // Texture coordinate attribute (location = 2) - half-precision
    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R16G16_SFLOAT;
    attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

    // Tangent attribute (location = 3) - half-precision
    attributeDescriptions[3].binding = 0;
    attributeDescriptions[3].location = 3;
    attributeDescriptions[3].format = VK_FORMAT_R16G16B16_SFLOAT;
    attributeDescriptions[3].offset = offsetof(Vertex, tangent);

    // Bitangent attribute (location = 4) - half-precision
    attributeDescriptions[4].binding = 0;
    attributeDescriptions[4].location = 4;
    attributeDescriptions[4].format = VK_FORMAT_R16G16B16_SFLOAT;
    attributeDescriptions[4].offset = offsetof(Vertex, bitangent);

    return attributeDescriptions;
}

vector<VkVertexInputAttributeDescription> Vertex::getAttributeDescriptionsAnimated()
{
    /*
     * ANIMATED VERTEX INPUT ATTRIBUTE DESCRIPTIONS (WITH BONE DATA):
     *
     * Uses half-precision formats for geometric attributes while keeping
     * full precision for bone weights and indices for animation accuracy.
     */

    vector<VkVertexInputAttributeDescription> attributeDescriptions(7);

    // Position attribute (location = 0) - half-precision
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R16G16B16_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, position);

    // Normal attribute (location = 1) - half-precision
    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R16G16B16_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, normal);

    // Texture coordinate attribute (location = 2) - half-precision
    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R16G16_SFLOAT;
    attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

    // Tangent attribute (location = 3) - half-precision
    attributeDescriptions[3].binding = 0;
    attributeDescriptions[3].location = 3;
    attributeDescriptions[3].format = VK_FORMAT_R16G16B16_SFLOAT;
    attributeDescriptions[3].offset = offsetof(Vertex, tangent);

    // Bitangent attribute (location = 4) - half-precision
    attributeDescriptions[4].binding = 0;
    attributeDescriptions[4].location = 4;
    attributeDescriptions[4].format = VK_FORMAT_R16G16B16_SFLOAT;
    attributeDescriptions[4].offset = offsetof(Vertex, bitangent);

    // Bone weights attribute (location = 5) - full precision for accuracy
    attributeDescriptions[5].binding = 0;
    attributeDescriptions[5].location = 5;
    attributeDescriptions[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[5].offset = offsetof(Vertex, boneWeights);

    // Bone indices attribute (location = 6) - full precision for compatibility
    attributeDescriptions[6].binding = 0;
    attributeDescriptions[6].location = 6;
    attributeDescriptions[6].format = VK_FORMAT_R32G32B32A32_SINT;
    attributeDescriptions[6].offset = offsetof(Vertex, boneIndices);

    return attributeDescriptions;
}

VkVertexInputBindingDescription Vertex::getBindingDescription()
{
    /*
     * VULKAN VERTEX INPUT BINDING DESCRIPTION:
     *
     * This describes how the f16-optimized vertex data is bound to the graphics pipeline:
     * - binding: binding point index (0 for single vertex buffer)
     * - stride: size of each vertex in bytes (~60 bytes - significant reduction from legacy)
     * - inputRate: VK_VERTEX_INPUT_RATE_VERTEX for per-vertex data
     *
     * The optimized stride provides significant memory bandwidth savings.
     */

    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
}

} // namespace hlab