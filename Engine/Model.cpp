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

Model::Model(Model&& other) noexcept
    : ctx_(other.ctx_), meshes_(std::move(other.meshes_)), materials_(std::move(other.materials_)),
      textures_(std::move(other.textures_)), textureFilenames_(std::move(other.textureFilenames_)),
      textureSRgb_(std::move(other.textureSRgb_)), rootNode_(std::move(other.rootNode_)),
      animation_(std::move(other.animation_)), name_(std::move(other.name_)),
      globalInverseTransform_(other.globalInverseTransform_),
      boundingBoxMin_(other.boundingBoxMin_), boundingBoxMax_(other.boundingBoxMax_),
      materialUBO_(std::move(other.materialUBO_)),
      materialDescriptorSets_(std::move(other.materialDescriptorSets_)), visible_(other.visible_),
      modelMatrix_(other.modelMatrix_)
{
    // Reset moved-from object to safe state
    other.globalInverseTransform_ = mat4(1.0f);
    other.boundingBoxMin_ = vec3(FLT_MAX);
    other.boundingBoxMax_ = vec3(-FLT_MAX);
    other.visible_ = true;
    other.modelMatrix_ = mat4(1.0f);
}

Model::~Model()
{
    cleanup();
}

void Model::createDescriptorSets(Sampler& sampler, Image2D& dummyTexture)
{
    for (size_t i = 0; i < materials_.size(); i++) {
        auto& mat = materials_[i];
        materialUBO_.emplace_back(ctx_, mat.ubo_);
    }

    for (auto& t : textures_) {
        t.setSampler(sampler.handle());
    }

    materialDescriptorSets_.resize(materials_.size());
    for (size_t i = 0; i < materials_.size(); i++) {
        auto& mat = materials_[i];
        auto& b1 = mat.ubo_.baseColorTextureIndex_ < 0
                       ? dummyTexture
                       : getTexture(mat.ubo_.baseColorTextureIndex_);
        auto& b2 = mat.ubo_.emissiveTextureIndex_ < 0 ? dummyTexture
                                                      : getTexture(mat.ubo_.emissiveTextureIndex_);
        auto& b3 = mat.ubo_.normalTextureIndex_ < 0 ? dummyTexture
                                                    : getTexture(mat.ubo_.normalTextureIndex_);
        auto& b4 = mat.ubo_.opacityTextureIndex_ < 0 ? dummyTexture
                                                     : getTexture(mat.ubo_.opacityTextureIndex_);
        auto& b5 = mat.ubo_.metallicRoughnessTextureIndex_ < 0
                       ? dummyTexture
                       : getTexture(mat.ubo_.metallicRoughnessTextureIndex_);
        auto& b6 = mat.ubo_.occlusionTextureIndex_ < 0
                       ? dummyTexture
                       : getTexture(mat.ubo_.occlusionTextureIndex_);
        materialDescriptorSets_[i].create(ctx_, {materialUBO_[i].resourceBinding(),
                                                 b1.resourceBinding(), b2.resourceBinding(),
                                                 b3.resourceBinding(), b4.resourceBinding(),
                                                 b5.resourceBinding(), b6.resourceBinding()});
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

    // for (auto& material : materials_) {
    //     material.cleanup(ctx_.device());
    // }

    for (auto& texture : textures_) {
        texture.cleanup();
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