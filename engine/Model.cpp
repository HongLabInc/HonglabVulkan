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

Model::Model(Context& ctx) : ctx_(ctx), textureManager_(ctx)
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

void Model::createDescriptorSets(Sampler& sampler, Image2D& dummyTexture)
{
    for (auto& t : textures_) {
        t->setSampler(sampler.handle());
    }

    textureManager_.textures_ = std::move(textures_);

    // Create single large storage buffer for all materials (bindless)
    if (!materials_.empty()) {
        materialStorageBuffer_ = make_unique<StorageBuffer>(ctx_);
        VkDeviceSize totalSize = sizeof(MaterialUBO) * materials_.size();
        materialStorageBuffer_->create(totalSize);

        vector<MaterialUBO> materialData;
        materialData.reserve(materials_.size());
        for (const auto& material : materials_) {
            materialData.push_back(material.ubo_);
        }
        materialStorageBuffer_->copyData(materialData.data(), totalSize);

        // Create single descriptor set for all materials
        materialDescriptorSet_.create(ctx_, {*materialStorageBuffer_, textureManager_});
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

    if (materialStorageBuffer_) {
        materialStorageBuffer_->cleanup();
        materialStorageBuffer_.reset();
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