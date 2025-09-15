# F16 Vertex Format Usage Guide

## Overview

The `Vertex` class provides half-precision (16-bit) floating point support for vertex attributes, significantly reducing memory bandwidth while maintaining sufficient precision for most rendering scenarios. The legacy format has been removed to provide a single, clear implementation.

## Key Benefits

- **Memory Bandwidth Reduction**: ~32% reduction in vertex memory usage (vs legacy 88-byte format)
- **Cache Performance**: Better GPU cache utilization due to smaller vertex size
- **Modern GPU Support**: Excellent f16 support on GPUs from 2016 onwards
- **Quality Preservation**: Sufficient precision for most geometric attributes
- **Single Implementation**: No confusion between multiple vertex formats

## When to Use F16 Vertices

### Recommended Use Cases
- **All modern rendering**: F16 is now the default and recommended format
- **Static Meshes**: Excellent for non-animated geometry
- **Animated Meshes**: Full support with f32 bone weights for accuracy
- **Terrain Rendering**: Perfect for large-scale terrain meshes
- **Particle Systems**: Ideal for particle vertex data
- **Mobile/VR Rendering**: Critical for bandwidth-constrained platforms

### Precision Considerations
- **Coordinate Range**: Suitable for world coordinates up to ~65k units
- **Texture Coordinates**: Excellent precision for [0,1] UV range
- **Normals**: Perfect for normalized vectors
- **Animation**: Bone weights/indices remain f32 for accuracy

## Usage Examples

### Basic Usage

```cpp
#include "engine/Vertex.h"

// Create vertex directly (f16 format)
Vertex vertex(
    vec3(1.0f, 2.0f, 3.0f),  // position
    vec3(0.0f, 1.0f, 0.0f),  // normal
    vec2(0.5f, 0.5f)         // texCoord
);

// Access values (automatic f16 to f32 conversion)
vec3 position = vertex.getPosition();
vec3 normal = vertex.getNormal();
vec2 texCoord = vertex.getTexCoord();

// Modify values (automatic f32 to f16 conversion)
vertex.setPosition(vec3(4.0f, 5.0f, 6.0f));
vertex.setNormal(normalize(vec3(1.0f, 1.0f, 0.0f)));
```

### Vulkan Pipeline Setup

```cpp
// Get attribute descriptions for f16 vertex format
auto attributeDescriptions = Vertex::getAttributeDescriptionsAnimated();
auto bindingDescription = Vertex::getBindingDescription();

// Use in pipeline creation
VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
vertexInputInfo.vertexBindingDescriptionCount = 1;
vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
vertexInputInfo.vertexAttributeDescriptionCount = attributeDescriptions.size();
vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
```

### Animated Vertices

```cpp
// Create vertex with animation data
Vertex animatedVertex(
    vec3(1.0f, 2.0f, 3.0f),    // position
    vec3(0.0f, 1.0f, 0.0f),    // normal
    vec2(0.5f, 0.5f),          // texCoord
    vec4(1.0f, 0.0f, 0.0f, 0.0f),  // bone weights (f32 precision)
    ivec4(0, -1, -1, -1)       // bone indices (i32 precision)
);

// Add bone data
animatedVertex.addBoneData(1, 0.3f);
animatedVertex.addBoneData(2, 0.2f);
animatedVertex.normalizeBoneWeights();
```

### Shader Considerations

In your vertex shaders, all f16 attributes are automatically converted to f32:

```glsl
#version 450

// These inputs receive f16 data but are converted to f32 automatically
layout(location = 0) in vec3 inPosition;   // From VK_FORMAT_R16G16B16_SFLOAT
layout(location = 1) in vec3 inNormal;     // From VK_FORMAT_R16G16B16_SFLOAT
layout(location = 2) in vec2 inTexCoord;   // From VK_FORMAT_R16G16_SFLOAT
layout(location = 3) in vec3 inTangent;    // From VK_FORMAT_R16G16B16_SFLOAT
layout(location = 4) in vec3 inBitangent;  // From VK_FORMAT_R16G16B16_SFLOAT

// Animation data remains full precision
layout(location = 5) in vec4 inBoneWeights; // VK_FORMAT_R32G32B32A32_SFLOAT
layout(location = 6) in ivec4 inBoneIndices; // VK_FORMAT_R32G32B32A32_SINT

void main() {
    // Use normally - GPU handles f16 to f32 conversion automatically
    vec4 worldPos = model * vec4(inPosition, 1.0);
    // ... rest of vertex shader
}
```

## Memory Layout Details

### F16 Optimized Vertex (~60 bytes)
```
Offset  Size  Type     Attribute
0       6     hvec3    position (f16)
6       6     hvec3    normal (f16)
12      4     hvec2    texCoord (f16)
16      6     hvec3    tangent (f16)
22      6     hvec3    bitangent (f16)
28      16    vec4     boneWeights (f32)
44      16    ivec4    boneIndices (i32)
```

### Memory Savings
- **Legacy format**: 88 bytes per vertex
- **F16 format**: ~60 bytes per vertex
- **Savings**: 32% reduction in memory bandwidth

## Precision Analysis

### Half-Precision Range and Precision
- **Range**: ±65,504 (sufficient for most world coordinates)
- **Precision**: ~3-4 decimal places
- **Smallest increment**: ~0.0006 near 1.0

### Attribute Suitability
- ? **Normals**: Excellent (normalized vectors work perfectly with f16)
- ? **Texture Coordinates**: Ideal (typically in [0,1] range)
- ? **Tangent/Bitangent**: Good (normalized vectors)
- ? **Position**: Good for most cases (check world scale if >30k units)
- ? **Animation Data**: Full precision maintained (f32)

## Performance Considerations

### Memory Bandwidth Savings
```cpp
// Example: 1M vertex mesh
const size_t vertexCount = 1000000;
size_t f16Memory = vertexCount * sizeof(Vertex);      // ~57 MB
size_t legacyMemory = vertexCount * 88;               // ~84 MB
// Bandwidth reduction: ~32%
```

### GPU Cache Efficiency
- More vertices fit in GPU cache lines
- Reduced memory controller pressure
- Better performance on bandwidth-limited scenarios

## Content Pipeline Integration

### Asset Processing
```cpp
// Convert mesh data to f16 vertices
std::vector<Vertex> processedVertices;
for (const auto& rawVertex : rawVertexData) {
    processedVertices.emplace_back(
        rawVertex.position,  // Automatically packed to f16
        rawVertex.normal,    // Automatically packed to f16
        rawVertex.texCoord   // Automatically packed to f16
    );
}
```

### Quality Validation
```cpp
// Validate precision for your specific use case
Vertex testVertex;
testVertex.setPosition(originalPosition);
vec3 retrieved = testVertex.getPosition();
vec3 error = abs(originalPosition - retrieved);
if (glm::length(error) > acceptableThreshold) {
    // Handle precision requirements
}
```

## Hardware Compatibility

### Modern Support (2016+)
- NVIDIA Pascal and newer
- AMD GCN 4.0 and newer  
- Intel Gen9 and newer
- All mobile GPUs supporting Vulkan

### Legacy Handling
- Older GPUs convert f16 to f32 internally
- No performance penalty, just no bandwidth savings
- Graceful degradation with identical visual results

## Best Practices

1. **Use by Default**: F16 is now the standard vertex format
2. **Check Coordinate Scale**: Ensure world coordinates fit in f16 range
3. **Validate Quality**: Test visual quality with your specific content
4. **Profile Performance**: Measure actual bandwidth savings in your application
5. **Leverage Automation**: Let the conversion system handle packing/unpacking

## Migration from Legacy Code

If you have existing code that expected the old vertex format:

### Before (Legacy)
```cpp
// Old direct member access (no longer available)
vertex.position = vec3(1, 2, 3);         // ERROR: No direct access
vec3 pos = vertex.position;              // ERROR: No direct access
```

### After (F16)
```cpp
// Use setter/getter methods
vertex.setPosition(vec3(1, 2, 3));       // Automatic f32 to f16 packing
vec3 pos = vertex.getPosition();         // Automatic f16 to f32 unpacking
```

## API Reference

### Constructors
```cpp
Vertex();                                           // Default constructor
Vertex(const vec3& pos, const vec3& norm, const vec2& tex);  // Basic vertex
Vertex(const vec3& pos, const vec3& norm, const vec2& tex, 
       const vec4& weights, const ivec4& indices);  // Animated vertex
```

### Accessor Methods
```cpp
vec3 getPosition() const;
vec3 getNormal() const;
vec2 getTexCoord() const;
vec3 getTangent() const;
vec3 getBitangent() const;
```

### Setter Methods
```cpp
void setPosition(const vec3& pos);
void setNormal(const vec3& norm);
void setTexCoord(const vec2& tex);
void setTangent(const vec3& tan);
void setBitangent(const vec3& bitan);
```

### Animation Methods
```cpp
void addBoneData(uint32_t boneIndex, float weight);
void normalizeBoneWeights();
bool hasValidBoneData() const;
```

### Static Methods
```cpp
static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptionsBasic();
static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptionsAnimated();
static VkVertexInputBindingDescription getBindingDescription();
```

This unified f16 vertex format provides excellent performance with a clean, maintainable API that eliminates confusion while delivering significant memory bandwidth optimizations.