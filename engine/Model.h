#pragma once

#include "Context.h"
#include "Material.h"
#include "Mesh.h"
#include "ModelNode.h"
#include "Sampler.h"
#include "Image2D.h"
#include "TextureManager.h"
#include "Vertex.h"
#include "VulkanTools.h"
#include "Animation.h"
#include "StorageBuffer.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace hlab {

using namespace std;
using namespace glm;

class Model
{
    friend class ModelLoader;

  public:
    Model(Context& ctx);
    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;
    Model& operator=(Model&&) = delete;
    ~Model();

    void cleanup();
    void createVulkanResources();

    void createDescriptorSets(Sampler& sampler, Image2D& dummyTexture);

    // Animation methods - ADD THESE
    void updateAnimation(float deltaTime);
    bool hasAnimations() const
    {
        return animation_ && animation_->hasAnimations();
    }
    bool hasBones() const
    {
        return animation_ && animation_->hasBones();
    }
    uint32_t getAnimationCount() const
    {
        return animation_ ? animation_->getAnimationCount() : 0;
    }
    uint32_t getBoneCount() const
    {
        return animation_ ? animation_->getBoneCount() : 0;
    }

    // Animation playback control - ADD THESE
    void playAnimation()
    {
        if (animation_)
            animation_->play();
    }
    void pauseAnimation()
    {
        if (animation_)
            animation_->pause();
    }
    void stopAnimation()
    {
        if (animation_)
            animation_->stop();
    }
    bool isAnimationPlaying() const
    {
        return animation_ && animation_->isPlaying();
    }
    void setAnimationIndex(uint32_t index)
    {
        if (animation_)
            animation_->setAnimationIndex(index);
    }
    void setAnimationSpeed(float speed)
    {
        if (animation_)
            animation_->setPlaybackSpeed(speed);
    }
    void setAnimationLooping(bool loop)
    {
        if (animation_)
            animation_->setLooping(loop);
    }

    // Bone matrices for shaders - ADD THESE
    const vector<mat4>& getBoneMatrices() const
    {
        static const vector<mat4> empty;
        return animation_ ? animation_->getBoneMatrices() : empty;
    }
    Animation* getAnimation() const
    {
        return animation_.get();
    } // Direct access if needed

    vector<Mesh>& meshes()
    {
        return meshes_;
    }
    vector<Material>& materials()
    {
        return materials_;
    }
    uint32_t numMaterials() const
    {
        return uint32_t(materials_.size());
    }
    ModelNode* rooNode() const
    {
        return rootNode_.get();
    }
    vec3 boundingBoxMin() const
    {
        return boundingBoxMin_;
    }
    vec3 boundingBoxMax() const
    {
        return boundingBoxMax_;
    }
    Image2D& getTexture(int index)
    {
        return *textureManager_.textures_[index];
    }

    const DescriptorSet& materialDescriptorSet() const
    {
        return materialDescriptorSet_;
    }

    void loadFromModelFile(const string& modelFilename, bool readBistroObj);

    auto name() -> string&
    {
        return name_;
    }

    auto visible() -> bool&
    {
        return visible_;
    }

    auto modelMatrix() -> mat4&
    {
        return modelMatrix_;
    }

    auto coeffs() -> float*
    {
        return coeffs_;
    }

  private:
    Context& ctx_;

    // Model asset data
    vector<Mesh> meshes_;
    vector<Material> materials_;

    TextureManager textureManager_;
    vector<unique_ptr<Image2D>> textures_;
    vector<string> textureFilenames_;
    vector<bool> textureSRgb_; // sRGB 여부

    unique_ptr<ModelNode> rootNode_;
    unique_ptr<Animation> animation_;

    mat4 globalInverseTransform_ = mat4(1.0f);

    // Bounding box
    vec3 boundingBoxMin_ = vec3(FLT_MAX);
    vec3 boundingBoxMax_ = vec3(-FLT_MAX);

    unique_ptr<StorageBuffer>
        materialStorageBuffer_;           // Single large StorageBuffer for all materials (bindless)
    DescriptorSet materialDescriptorSet_; // Single descriptor set for all materials

    string name_{};
    bool visible_ = true;
    mat4 modelMatrix_ = mat4(1.0f);
    float coeffs_[16] = {0.0f}; // 여러가지 옵션에 사용

    void calculateBoundingBox();
};

} // namespace hlab