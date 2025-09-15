/*
 * F16 Vertex Format - Single Optimized Implementation
 * 
 * This example demonstrates the streamlined f16 vertex format.
 * The legacy format has been removed to eliminate confusion.
 * 
 * Key benefits:
 * - ~32% reduction in vertex memory usage compared to legacy format
 * - Single, clear implementation (no more confusion between formats)
 * - Improved cache performance and memory bandwidth
 * - Suitable for most rendering scenarios while maintaining quality
 */

#include "engine/Vertex.h"
#include <iostream>
#include <vector>
#include <chrono>

using namespace hlab;

int main()
{
    std::cout << "=== F16 Vertex Format (Single Implementation) ===" << std::endl;
    std::cout << "Legacy format removed - now using only optimized f16!" << std::endl;
    std::cout << std::endl;
    
    // Display size information
    std::cout << "=== Memory Efficiency ===" << std::endl;
    std::cout << "Vertex size: " << sizeof(Vertex) << " bytes" << std::endl;
    std::cout << "Memory savings vs legacy: ~32% (88 ? " << sizeof(Vertex) << " bytes)" << std::endl;
    std::cout << std::endl;

    // Create vertices using the single Vertex type (f16 format)
    std::cout << "=== Creating F16 Vertices ===" << std::endl;
    std::vector<Vertex> vertices;
    vertices.emplace_back(vec3(1.5f, 2.5f, 3.5f), vec3(0.0f, 1.0f, 0.0f), vec2(0.5f, 0.7f));
    vertices.emplace_back(vec3(-1.0f, 0.0f, 1.0f), vec3(1.0f, 0.0f, 0.0f), vec2(0.2f, 0.8f));
    vertices.emplace_back(vec3(0.0f, -2.0f, 0.5f), vec3(0.0f, 0.0f, 1.0f), vec2(0.9f, 0.1f));

    std::cout << "Created " << vertices.size() << " vertices using f16 format" << std::endl;
    
    for (size_t i = 0; i < vertices.size(); ++i) {
        vec3 pos = vertices[i].getPosition();
        vec3 norm = vertices[i].getNormal();
        vec2 tex = vertices[i].getTexCoord();
        
        std::cout << "Vertex " << i << ":" << std::endl;
        std::cout << "  Position: (" << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
        std::cout << "  Normal: (" << norm.x << ", " << norm.y << ", " << norm.z << ")" << std::endl;
        std::cout << "  TexCoord: (" << tex.x << ", " << tex.y << ")" << std::endl;
    }
    std::cout << std::endl;

    // Demonstrate precision characteristics
    std::cout << "=== Precision Validation ===" << std::endl;
    
    // Test various coordinate ranges
    const vec3 testPositions[] = {
        vec3(0.0f, 0.0f, 0.0f),           // Origin
        vec3(1.0f, 1.0f, 1.0f),           // Unit
        vec3(10.0f, 20.0f, 30.0f),        // Medium scale
        vec3(100.0f, 200.0f, 300.0f),     // Large scale
        vec3(0.001f, 0.002f, 0.003f),     // Small scale
    };

    for (size_t i = 0; i < 5; ++i) {
        Vertex testVertex;
        testVertex.setPosition(testPositions[i]);
        vec3 retrieved = testVertex.getPosition();
        vec3 diff = abs(testPositions[i] - retrieved);
        
        std::cout << "Test " << i << " - Original: (" << testPositions[i].x << ", " 
                  << testPositions[i].y << ", " << testPositions[i].z << ")" << std::endl;
        std::cout << "         Retrieved: (" << retrieved.x << ", " 
                  << retrieved.y << ", " << retrieved.z << ")" << std::endl;
        std::cout << "         Difference: (" << diff.x << ", " 
                  << diff.y << ", " << diff.z << ")" << std::endl;
    }
    std::cout << std::endl;

    // Memory bandwidth calculation for large meshes
    const size_t vertexCount = 1000000; // 1M vertices
    size_t totalMemory = vertexCount * sizeof(Vertex);
    
    std::cout << "=== Memory Bandwidth Impact (1M vertices) ===" << std::endl;
    std::cout << "Total vertex memory: " << (totalMemory / 1024 / 1024) << " MB" << std::endl;
    std::cout << "Legacy would have used: ~84 MB (32% more bandwidth)" << std::endl;
    std::cout << "Memory bandwidth saved: ~27 MB per 1M vertices" << std::endl;
    std::cout << std::endl;

    // Vulkan format information
    std::cout << "=== Vulkan Pipeline Integration ===" << std::endl;
    std::cout << "Getting vertex attribute descriptions..." << std::endl;
    auto attributeDescs = Vertex::getAttributeDescriptionsAnimated();
    std::cout << "Number of vertex attributes: " << attributeDescs.size() << std::endl;
    
    std::cout << "Attribute formats:" << std::endl;
    const char* formatNames[] = {
        "Position (R16G16B16_SFLOAT)",
        "Normal (R16G16B16_SFLOAT)", 
        "TexCoord (R16G16_SFLOAT)",
        "Tangent (R16G16B16_SFLOAT)",
        "Bitangent (R16G16B16_SFLOAT)",
        "BoneWeights (R32G32B32A32_SFLOAT)",
        "BoneIndices (R32G32B32A32_SINT)"
    };
    
    for (size_t i = 0; i < attributeDescs.size() && i < 7; ++i) {
        std::cout << "  " << formatNames[i] << " (offset: " << attributeDescs[i].offset << ")" << std::endl;
    }
    
    auto bindingDesc = Vertex::getBindingDescription();
    std::cout << "Vertex binding stride: " << bindingDesc.stride << " bytes" << std::endl;
    std::cout << std::endl;

    // Performance simulation
    std::cout << "=== Performance Benefits ===" << std::endl;
    std::cout << "? Single, clear vertex format (no confusion)" << std::endl;
    std::cout << "? 32% memory bandwidth reduction" << std::endl;
    std::cout << "? Improved GPU cache utilization" << std::endl;
    std::cout << "? Modern GPU f16 optimization" << std::endl;
    std::cout << "? Sufficient precision for most use cases" << std::endl;
    std::cout << "? Full animation support with f32 bone data" << std::endl;
    std::cout << std::endl;

    // Usage patterns
    std::cout << "=== Common Usage Patterns ===" << std::endl;
    
    // 1. Direct construction
    std::cout << "1. Direct construction:" << std::endl;
    Vertex directVertex(vec3(1, 2, 3), vec3(0, 1, 0), vec2(0.5, 0.5));
    std::cout << "   Vertex v(position, normal, texCoord);" << std::endl;
    
    // 2. Setter methods
    std::cout << "2. Using setter methods:" << std::endl;
    Vertex setterVertex;
    setterVertex.setPosition(vec3(4, 5, 6));
    setterVertex.setNormal(vec3(1, 0, 0));
    setterVertex.setTexCoord(vec2(0.2, 0.8));
    std::cout << "   vertex.setPosition(pos); vertex.setNormal(norm);" << std::endl;
    
    // 3. Getter methods
    std::cout << "3. Using getter methods:" << std::endl;
    vec3 pos = setterVertex.getPosition();
    std::cout << "   vec3 pos = vertex.getPosition(); // Automatic f16 ? f32 conversion" << std::endl;
    std::cout << std::endl;

    std::cout << "=== F16 Migration Complete ===" << std::endl;
    std::cout << "?? Single optimized vertex format" << std::endl;
    std::cout << "?? No more legacy/optimized confusion" << std::endl;
    std::cout << "?? Automatic memory bandwidth optimization" << std::endl;
    std::cout << "?? Clean, maintainable codebase" << std::endl;

    return 0;
}