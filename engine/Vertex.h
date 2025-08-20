#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <vulkan/vulkan.h>

using namespace glm;

namespace hlab {

class Vertex
{
  public:
    /*
     * VULKAN VERTEX BUFFER LAYOUT REQUIREMENTS:
     *
     * Unlike uniform buffers (std140), vertex buffers have more flexible alignment rules:
     * - Vertex attributes are tightly packed in memory
     * - Each attribute must be aligned to its component type (float = 4 bytes)
     * - No padding is automatically inserted between attributes
     * - Alignment is handled by the vertex input attribute descriptions
     *
     * Component type alignments:
     * - float (4 bytes): 4-byte alignment
     * - vec2 (8 bytes): 4-byte alignment (2 floats)
     * - vec3 (12 bytes): 4-byte alignment (3 floats)
     * - vec4 (16 bytes): 4-byte alignment (4 floats)
     * - ivec4 (16 bytes): 4-byte alignment (4 ints)
     *
     * Updated layout with skeletal animation support:
     * - position: vec3 (12 bytes) - offset 0, 4-byte aligned
     * - normal: vec3 (12 bytes) - offset 12, 4-byte aligned
     * - texCoord: vec2 (8 bytes) - offset 24, 4-byte aligned
     * - tangent: vec3 (12 bytes) - offset 32, 4-byte aligned
     * - bitangent: vec3 (12 bytes) - offset 44, 4-byte aligned
     * - boneWeights: vec4 (16 bytes) - offset 56, 4-byte aligned
     * - boneIndices: ivec4 (16 bytes) - offset 72, 4-byte aligned
     * Total size: 88 bytes
     *
     * This layout supports both regular rendering and skeletal animation
     * while maintaining good cache performance for vertex processing.
     */

    alignas(4) vec3 position;  // 12 bytes - vertex position in object space
    alignas(4) vec3 normal;    // 12 bytes - vertex normal vector
    alignas(4) vec2 texCoord;  // 8 bytes - texture coordinates (UV)
    alignas(4) vec3 tangent;   // 12 bytes - tangent vector for normal mapping
    alignas(4) vec3 bitangent; // 12 bytes - bitangent vector for normal mapping

    // Skeletal animation support
    alignas(4) vec4 boneWeights;  // 16 bytes - bone influence weights (up to 4 bones per vertex)
    alignas(4) ivec4 boneIndices; // 16 bytes - bone indices (up to 4 bones per vertex)

    // Default constructor
    Vertex()
        : position(0.0f), normal(0.0f, 1.0f, 0.0f), texCoord(0.0f), tangent(1.0f, 0.0f, 0.0f),
          bitangent(0.0f, 0.0f, 1.0f), boneWeights(0.0f), boneIndices(-1)
    {
    }

    // Constructor for regular vertices (without animation)
    Vertex(const vec3& pos, const vec3& norm, const vec2& tex)
        : position(pos), normal(norm), texCoord(tex), tangent(1.0f, 0.0f, 0.0f),
          bitangent(0.0f, 0.0f, 1.0f), boneWeights(0.0f), boneIndices(-1)
    {
    }

    // Constructor for animated vertices
    Vertex(const vec3& pos, const vec3& norm, const vec2& tex, const vec4& weights,
           const ivec4& indices)
        : position(pos), normal(norm), texCoord(tex), tangent(1.0f, 0.0f, 0.0f),
          bitangent(0.0f, 0.0f, 1.0f), boneWeights(weights), boneIndices(indices)
    {
    }

    // Methods for bone weight management
    void addBoneData(uint32_t boneIndex, float weight);
    void normalizeBoneWeights();
    bool hasValidBoneData() const;

    // Static methods for Vulkan vertex input configuration
    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
    static VkVertexInputBindingDescription getBindingDescription();

    // Static methods for different vertex configurations
    static std::vector<VkVertexInputAttributeDescription>
    getAttributeDescriptionsBasic(); // Without bone data
    static std::vector<VkVertexInputAttributeDescription>
    getAttributeDescriptionsAnimated(); // With bone data
};

// Compile-time validation of vertex structure layout
// (Must be outside class definition to avoid incomplete type issues)
static_assert(sizeof(vec3) == 12, "vec3 must be 12 bytes");
static_assert(sizeof(vec2) == 8, "vec2 must be 8 bytes");
static_assert(sizeof(vec4) == 16, "vec4 must be 16 bytes");
static_assert(sizeof(ivec4) == 16, "ivec4 must be 16 bytes");
static_assert(sizeof(Vertex) == 88,
              "Vertex size must be 88 bytes for optimal packing with animation support");

} // namespace hlab
