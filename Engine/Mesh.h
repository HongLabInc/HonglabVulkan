#pragma once

#include "Context.h"
#include "Material.h"
#include "Vertex.h"
#include "ViewFrustum.h"

#include <fstream>
#include <glm/glm.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

using namespace glm;
using namespace std;

namespace hlab {

class Mesh
{
  public:
    Mesh()
    {
    }

    Mesh(Mesh&& other) noexcept
        : name_(std::move(other.name_)), vertices_(std::move(other.vertices_)),
          indices_(std::move(other.indices_)), materialIndex_(other.materialIndex_),
          vertexBuffer_(other.vertexBuffer_), vertexMemory_(other.vertexMemory_),
          indexBuffer_(other.indexBuffer_), indexMemory_(other.indexMemory_),
          minBounds(other.minBounds), maxBounds(other.maxBounds), worldBounds(other.worldBounds),
          isCulled(other.isCulled), noTextureCoords(other.noTextureCoords)
    {
        // Reset moved-from object to safe state
        other.vertexBuffer_ = VK_NULL_HANDLE;
        other.vertexMemory_ = VK_NULL_HANDLE;
        other.indexBuffer_ = VK_NULL_HANDLE;
        other.indexMemory_ = VK_NULL_HANDLE;
        other.materialIndex_ = 0;
        other.minBounds = vec3(FLT_MAX);
        other.maxBounds = vec3(-FLT_MAX);
        other.isCulled = false;
        other.noTextureCoords = false;
    }

    Mesh& operator=(Mesh&& other) noexcept
    {
        if (this != &other) {
            // Note: We cannot safely cleanup existing Vulkan resources here
            // because we don't have access to VkDevice
            // The user must call cleanup() before move assignment if needed

            // Move all data members
            name_ = std::move(other.name_);
            vertices_ = std::move(other.vertices_);
            indices_ = std::move(other.indices_);
            materialIndex_ = other.materialIndex_;

            // Transfer Vulkan resource ownership
            vertexBuffer_ = other.vertexBuffer_;
            vertexMemory_ = other.vertexMemory_;
            indexBuffer_ = other.indexBuffer_;
            indexMemory_ = other.indexMemory_;

            // Copy other members
            minBounds = other.minBounds;
            maxBounds = other.maxBounds;
            worldBounds = other.worldBounds;
            isCulled = other.isCulled;
            noTextureCoords = other.noTextureCoords;

            // Reset moved-from object to safe state
            other.vertexBuffer_ = VK_NULL_HANDLE;
            other.vertexMemory_ = VK_NULL_HANDLE;
            other.indexBuffer_ = VK_NULL_HANDLE;
            other.indexMemory_ = VK_NULL_HANDLE;
            other.materialIndex_ = 0;
            other.minBounds = vec3(FLT_MAX);
            other.maxBounds = vec3(-FLT_MAX);
            other.isCulled = false;
            other.noTextureCoords = false;
        }
        return *this;
    }

    string name_ = {};
    vector<Vertex> vertices_{};
    vector<uint32_t> indices_{};
    uint32_t materialIndex_ = 0;

    // Vulkan buffers
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory vertexMemory_ = VK_NULL_HANDLE;
    VkBuffer indexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory indexMemory_ = VK_NULL_HANDLE;

    // Bounding box for culling
    vec3 minBounds = vec3(FLT_MAX);
    vec3 maxBounds = vec3(-FLT_MAX);

    void createBuffers(Context& ctx);
    void cleanup(VkDevice device);
    void calculateBounds(); // Made public

    // Update Mesh::updateWorldBounds implementation
    void updateWorldBounds(const glm::mat4& modelMatrix);

    // World-space bounding box (updated when model matrix changes)
    AABB worldBounds{};

    // Check if mesh should be culled
    bool isCulled = false;
    bool noTextureCoords = false;

    // Binary file I/O methods
    bool readFromBinaryFileStream(std::ifstream& stream);
    bool writeToBinaryFileStream(std::ofstream& stream) const;

  private:
    // Helper methods for binary I/O
    template <typename T>
    bool writeValue(std::ofstream& stream, const T& value) const;

    template <typename T>
    bool readValue(std::ifstream& stream, T& value);

    bool writeString(std::ofstream& stream, const std::string& str) const;
    bool readString(std::ifstream& stream, std::string& str);

    template <typename T>
    bool writeVector(std::ofstream& stream, const std::vector<T>& vec) const;

    template <typename T>
    bool readVector(std::ifstream& stream, std::vector<T>& vec);
};

// Template implementations (must be in header for C++14)
template <typename T>
bool Mesh::writeValue(std::ofstream& stream, const T& value) const
{
    stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
    return stream.good();
}

template <typename T>
bool Mesh::readValue(std::ifstream& stream, T& value)
{
    stream.read(reinterpret_cast<char*>(&value), sizeof(T));
    return stream.good();
}

template <typename T>
bool Mesh::writeVector(std::ofstream& stream, const std::vector<T>& vec) const
{
    uint32_t size = static_cast<uint32_t>(vec.size());
    if (!writeValue(stream, size))
        return false;

    if (size > 0) {
        stream.write(reinterpret_cast<const char*>(vec.data()), size * sizeof(T));
        return stream.good();
    }
    return true;
}

template <typename T>
bool Mesh::readVector(std::ifstream& stream, std::vector<T>& vec)
{
    uint32_t size;
    if (!readValue(stream, size))
        return false;

    vec.resize(size);
    if (size > 0) {
        stream.read(reinterpret_cast<char*>(vec.data()), size * sizeof(T));
        return stream.good();
    }
    return true;
}

} // namespace hlab
