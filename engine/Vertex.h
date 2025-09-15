#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/packing.hpp>  // For half-precision support
#include <vector>
#include <vulkan/vulkan.h>

using namespace glm;

namespace hlab {

// Half-precision type aliases for clarity and consistency
using half = uint16_t;  // Using uint16_t to represent packed half-float values

// Custom packed half-precision vector types for optimal memory layout
struct alignas(2) hvec2 {
    half x, y;
    hvec2() : x(0), y(0) {}
    hvec2(half x_, half y_) : x(x_), y(y_) {}
};

struct alignas(2) hvec3 {
    half x, y, z;
    hvec3() : x(0), y(0), z(0) {}
    hvec3(half x_, half y_, half z_) : x(x_), y(y_), z(z_) {}
};

struct alignas(2) hvec4 {
    half x, y, z, w;
    hvec4() : x(0), y(0), z(0), w(0) {}
    hvec4(half x_, half y_, half z_, half w_) : x(x_), y(y_), z(z_), w(w_) {}
};

// Helper functions for half-precision conversion
inline half packHalf(float value) {
    return glm::packHalf1x16(value);
}

inline float unpackHalf(half value) {
    return glm::unpackHalf1x16(value);
}

inline hvec2 packHalf2(const vec2& value) {
    return hvec2(packHalf(value.x), packHalf(value.y));
}

inline vec2 unpackHalf2(const hvec2& value) {
    return vec2(unpackHalf(value.x), unpackHalf(value.y));
}

inline hvec3 packHalf3(const vec3& value) {
    return hvec3(packHalf(value.x), packHalf(value.y), packHalf(value.z));
}

inline vec3 unpackHalf3(const hvec3& value) {
    return vec3(unpackHalf(value.x), unpackHalf(value.y), unpackHalf(value.z));
}

/*
 * OPTIMIZED VERTEX STRUCTURE WITH F16 SUPPORT
 *
 * This vertex structure uses half-precision (16-bit) floating point for most attributes
 * to significantly reduce memory bandwidth and improve cache performance.
 *
 * Memory layout optimization with f16:
 * - position: hvec3 (6 bytes) - half-precision position, sufficient for most geometry
 * - normal: hvec3 (6 bytes) - half-precision normals work excellently due to normalized nature
 * - texCoord: hvec2 (4 bytes) - texture coordinates are ideal for f16 in [0,1] range
 * - tangent: hvec3 (6 bytes) - tangent space vectors work well with f16
 * - bitangent: hvec3 (6 bytes) - bitangent vectors work well with f16
 * - boneWeights: vec4 (16 bytes) - keep full precision for animation accuracy
 * - boneIndices: ivec4 (16 bytes) - keep full precision for compatibility
 * Total size: ~60 bytes (32% reduction from legacy 88 bytes)
 *
 * Benefits:
 * - Significant reduction in vertex memory bandwidth
 * - Improved cache efficiency
 * - Faster vertex processing on modern GPUs
 * - Maintains sufficient precision for most rendering scenarios
 */
class Vertex
{
  public:
    /*
     * VULKAN VERTEX BUFFER LAYOUT REQUIREMENTS:
     *
     * Unlike uniform buffers (std140), vertex buffers have more flexible alignment rules:
     * - Vertex attributes are tightly packed in memory
     * - Each attribute must be aligned to its component type
     * - No padding is automatically inserted between attributes
     * - Alignment is handled by the vertex input attribute descriptions
     *
     * Component type alignments for f16:
     * - half (2 bytes): 2-byte alignment
     * - hvec2 (4 bytes): 2-byte alignment (2 halfs)
     * - hvec3 (6 bytes): 2-byte alignment (3 halfs)
     * - hvec4 (8 bytes): 2-byte alignment (4 halfs)
     * - vec4 (16 bytes): 4-byte alignment (4 floats)
     * - ivec4 (16 bytes): 4-byte alignment (4 ints)
     *
     * Optimized layout with f16 and skeletal animation support:
     * - position: hvec3 (6 bytes) - offset 0, 2-byte aligned
     * - normal: hvec3 (6 bytes) - offset 6, 2-byte aligned
     * - texCoord: hvec2 (4 bytes) - offset 12, 2-byte aligned
     * - tangent: hvec3 (6 bytes) - offset 16, 2-byte aligned
     * - bitangent: hvec3 (6 bytes) - offset 22, 2-byte aligned
     * - boneWeights: vec4 (16 bytes) - offset 28, 4-byte aligned
     * - boneIndices: ivec4 (16 bytes) - offset 44, 4-byte aligned
     * Total size: ~60 bytes
     *
     * This layout provides excellent cache performance and memory bandwidth
     * while maintaining sufficient precision for high-quality rendering.
     */

    // Optimized layout using half-precision for geometric attributes
    hvec3 position;     // 6 bytes - half-precision position
    hvec3 normal;       // 6 bytes - half-precision normal
    hvec2 texCoord;     // 4 bytes - half-precision texture coordinates
    hvec3 tangent;      // 6 bytes - half-precision tangent
    hvec3 bitangent;    // 6 bytes - half-precision bitangent
    
    // Keep animation data at full precision for accuracy
    alignas(4) vec4 boneWeights;  // 16 bytes - full precision bone weights
    alignas(4) ivec4 boneIndices; // 16 bytes - full precision bone indices

    // Default constructor
    Vertex()
        : position(packHalf3(vec3(0.0f))),
          normal(packHalf3(vec3(0.0f, 1.0f, 0.0f))),
          texCoord(packHalf2(vec2(0.0f))),
          tangent(packHalf3(vec3(1.0f, 0.0f, 0.0f))),
          bitangent(packHalf3(vec3(0.0f, 0.0f, 1.0f))),
          boneWeights(0.0f),
          boneIndices(-1)
    {
    }

    // Constructor for regular vertices (without animation)
    Vertex(const vec3& pos, const vec3& norm, const vec2& tex)
        : position(packHalf3(pos)),
          normal(packHalf3(norm)),
          texCoord(packHalf2(tex)),
          tangent(packHalf3(vec3(1.0f, 0.0f, 0.0f))),
          bitangent(packHalf3(vec3(0.0f, 0.0f, 1.0f))),
          boneWeights(0.0f),
          boneIndices(-1)
    {
    }

    // Constructor for animated vertices
    Vertex(const vec3& pos, const vec3& norm, const vec2& tex, 
           const vec4& weights, const ivec4& indices)
        : position(packHalf3(pos)),
          normal(packHalf3(norm)),
          texCoord(packHalf2(tex)),
          tangent(packHalf3(vec3(1.0f, 0.0f, 0.0f))),
          bitangent(packHalf3(vec3(0.0f, 0.0f, 1.0f))),
          boneWeights(weights),
          boneIndices(indices)
    {
    }

    // Accessor methods for unpacked values
    vec3 getPosition() const { return unpackHalf3(position); }
    vec3 getNormal() const { return unpackHalf3(normal); }
    vec2 getTexCoord() const { return unpackHalf2(texCoord); }
    vec3 getTangent() const { return unpackHalf3(tangent); }
    vec3 getBitangent() const { return unpackHalf3(bitangent); }

    // Setter methods with automatic packing
    void setPosition(const vec3& pos) { position = packHalf3(pos); }
    void setNormal(const vec3& norm) { normal = packHalf3(norm); }
    void setTexCoord(const vec2& tex) { texCoord = packHalf2(tex); }
    void setTangent(const vec3& tan) { tangent = packHalf3(tan); }
    void setBitangent(const vec3& bitan) { bitangent = packHalf3(bitan); }

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
static_assert(sizeof(vec4) == 16, "vec4 must be 16 bytes");
static_assert(sizeof(ivec4) == 16, "ivec4 must be 16 bytes");
static_assert(sizeof(half) == 2, "half must be 2 bytes");
static_assert(sizeof(hvec2) == 4, "hvec2 must be 4 bytes");
static_assert(sizeof(hvec3) == 6, "hvec3 must be 6 bytes");
static_assert(sizeof(Vertex) <= 64,
              "Vertex size should be around 60 bytes for significant memory savings");

} // namespace hlab
