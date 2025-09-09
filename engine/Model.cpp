#include "Model.h"
#include "ModelNode.h"
#include "Vertex.h"
#include "Logger.h"
#include "ModelLoader.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <glm/gtx/string_cast.hpp>
#include <iostream>

namespace hlab {

using namespace std;
using namespace glm;

Model::Model(Context& ctx) : ctx_(ctx)
{
    rootNode_ = make_unique<ModelNode>();
    rootNode_->name = "Root";

    // Initialize animation system - ADD THIS
    animation_ = make_unique<Animation>();
}

Model::~Model()
{
    cleanup();
}

void Model::createDescriptorSets(Sampler& sampler, vector<MaterialUBO>& allMaterials,
                                 TextureManager& textureManager)
{
    for (auto& t : textures_) {
        t->setSampler(sampler.handle());
    }

    int materialBaseIndex = int(allMaterials.size());
    int textureBaseIndex = int(textureManager.textures_.size());

    // Append textures to textureManager.textures_
    textureManager.textures_.reserve(textureManager.textures_.size() + textures_.size());
    for (auto& texture : textures_) {
        textureManager.textures_.push_back(std::move(texture));
    }
    textures_.clear(); // Clear the source vector since we moved the textures

    // Create single large storage buffer for all materials (bindless)
    if (!materials_.empty()) {

        // Adjust all material texture indices by adding baseIndex if they are not -1
        for (auto& material : materials_) {
            if (material.ubo_.baseColorTextureIndex_ != -1) {
                material.ubo_.baseColorTextureIndex_ += textureBaseIndex;
            }
            if (material.ubo_.emissiveTextureIndex_ != -1) {
                material.ubo_.emissiveTextureIndex_ += textureBaseIndex;
            }
            if (material.ubo_.normalTextureIndex_ != -1) {
                material.ubo_.normalTextureIndex_ += textureBaseIndex;
            }
            if (material.ubo_.opacityTextureIndex_ != -1) {
                material.ubo_.opacityTextureIndex_ += textureBaseIndex;
            }
            if (material.ubo_.metallicRoughnessTextureIndex_ != -1) {
                material.ubo_.metallicRoughnessTextureIndex_ += textureBaseIndex;
            }
            if (material.ubo_.occlusionTextureIndex_ != -1) {
                material.ubo_.occlusionTextureIndex_ += textureBaseIndex;
            }
        }

        for (const auto& material : materials_) {
            allMaterials.push_back(material.ubo_);
        }

        // Iterate all meshes and add materialBaseIndex to mesh material index
        for (auto& mesh : meshes_) {
            mesh.materialIndex_ += materialBaseIndex;
        }
    }
}

void Model::createVulkanResources()
{
    // Create mesh buffers
    for (auto& mesh : meshes_) {
        mesh.createBuffers(ctx_);
    }

    // Create material uniform buffers
    // for (auto& material : materials_) {
    //    material.createUniformBuffer(ctx_);
    //    material.updateUniformBuffer();
    //}
}

void Model::loadFromModelFile(const string& modelFilename, bool readBistroObj)
{
    ModelLoader modelLoader(*this);
    modelLoader.loadFromModelFile(modelFilename, readBistroObj);
    createVulkanResources();
}

void Model::calculateBoundingBox()
{
    boundingBoxMin_ = vec3(FLT_MAX);
    boundingBoxMax_ = vec3(-FLT_MAX);

    for (const auto& mesh : meshes_) {
        boundingBoxMin_ = min(boundingBoxMin_, mesh.minBounds);
        boundingBoxMax_ = max(boundingBoxMax_, mesh.maxBounds);
    }
}

void Model::cleanup()
{
    for (auto& mesh : meshes_) {
        mesh.cleanup(ctx_.device());
    }

    for (auto& texture : textures_) {
        texture->cleanup();
    }

    meshes_.clear();
    materials_.clear();
}

void Model::updateAnimation(float deltaTime)
{
    if (animation_ && animation_->hasAnimations()) {
        animation_->updateAnimation(deltaTime);
    }
}

} // namespace hlab