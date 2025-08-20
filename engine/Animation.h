#pragma once

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <map>
#include <iostream>
#include <glm/gtc/type_ptr.hpp>
#include <assimp/scene.h>
#include <assimp/anim.h>

namespace hlab {

using namespace std;
using namespace glm;

template <typename T>
struct AnimationKey
{
    double time; // Time in animation (usually in seconds)
    T value;     // Value at this keyframe

    AnimationKey() : time(0.0)
    {
    }
    AnimationKey(double t, const T& v) : time(t), value(v)
    {
    }
};

using PositionKey = AnimationKey<vec3>;
using RotationKey = AnimationKey<quat>;
using ScaleKey = AnimationKey<vec3>;

struct AnimationChannel
{
    string nodeName; // Name of the target node/bone

    vector<PositionKey> positionKeys; // Position keyframes
    vector<RotationKey> rotationKeys; // Rotation keyframes
    vector<ScaleKey> scaleKeys;       // Scale keyframes

    // Interpolation methods for keyframes
    vec3 interpolatePosition(double time) const;
    quat interpolateRotation(double time) const;
    vec3 interpolateScale(double time) const;

  private:
    template <typename T>
    T interpolateKeys(const vector<AnimationKey<T>>& keys, double time) const;
};

struct Bone
{
    string name;              // Bone name
    int id;                   // Unique bone ID
    mat4 offsetMatrix;        // Inverse bind pose matrix
    mat4 finalTransformation; // Final transformation matrix
    int parentIndex;          // Parent bone index

    // Vertex weights influenced by this bone
    struct VertexWeight
    {
        uint32_t vertexId;
        float weight;
    };
    vector<VertexWeight> weights;

    Bone() : id(-1), offsetMatrix(1.0f), finalTransformation(1.0f), parentIndex(-1)
    {
    }
};

class Animation
{
  public:
    Animation();
    ~Animation();

    // Animation loading and processing
    void loadFromScene(const aiScene* scene);
    void processAnimations(const aiScene* scene);
    void processBones(const aiScene* scene);
    void buildSceneGraph(const aiScene* scene);

    void buildBoneHierarchy(const aiScene* scene);
    void assignGlobalBoneIds();

    void updateAnimation(float timeInSeconds);
    void setAnimationIndex(uint32_t index);
    void setPlaybackSpeed(float speed);
    void setLooping(bool loop);

    void calculateBoneTransforms(vector<mat4>& transforms, const string& nodeName = "",
                                 const mat4& parentTransform = mat4(1.0f));
    mat4 getNodeTransformation(const string& nodeName, double time) const;

    int getGlobalBoneIndex(const string& boneName) const;

    // Getters
    bool hasAnimations() const
    {
        return !animations_.empty();
    }
    bool hasBones() const
    {
        return !bones_.empty();
    }
    uint32_t getAnimationCount() const
    {
        return static_cast<uint32_t>(animations_.size());
    }
    uint32_t getBoneCount() const
    {
        return static_cast<uint32_t>(bones_.size());
    }
    float getDuration() const;
    float getCurrentTime() const
    {
        return currentTime_;
    }
    const string& getCurrentAnimationName() const;

    // Bone matrix access for shaders
    const vector<mat4>& getBoneMatrices() const
    {
        return boneMatrices_;
    }
    const mat4& getGlobalInverseTransform() const
    {
        return globalInverseTransform_;
    }

    // Animation state
    bool isPlaying() const
    {
        return isPlaying_;
    }
    void play()
    {
        isPlaying_ = true;
    }
    void pause()
    {
        isPlaying_ = false;
    }
    void stop()
    {
        isPlaying_ = false;
        currentTime_ = 0.0f;
    }

    void setGlobalInverseTransform(const mat4& transform)
    {
        globalInverseTransform_ = transform;
    }

  private:
    struct SceneNode;

    struct AnimationClip
    {
        string name;
        double duration;       // In seconds
        double ticksPerSecond; // Animation speed
        vector<AnimationChannel> channels;

        AnimationClip() : duration(0.0), ticksPerSecond(25.0)
        {
        }
    };

    struct SceneNode
    {
        string name;
        mat4 transformation;
        vector<unique_ptr<SceneNode>> children;
        SceneNode* parent = nullptr;

        SceneNode(const string& n) : name(n), transformation(1.0f)
        {
        }
    };

    vector<AnimationClip> animations_;
    vector<Bone> bones_;
    unordered_map<string, uint32_t> boneMapping_; // Bone name to index mapping

    unordered_map<string, int> globalBoneNameToId_; // Global bone name to ID mapping
    vector<string> globalBoneIdToName_;             // Global bone ID to name mapping

    // Playback state
    uint32_t currentAnimationIndex_;
    float currentTime_;
    float playbackSpeed_;
    bool isPlaying_;
    bool isLooping_;

    vector<mat4> boneMatrices_;   // Final bone transformation matrices
    mat4 globalInverseTransform_; // Global inverse transformation

    // Scene graph data
    unique_ptr<SceneNode> rootNode_;
    unordered_map<string, SceneNode*> nodeMapping_; // Quick node lookup

    // Helper methods
    void processAnimationChannel(const aiNodeAnim* nodeAnim, AnimationChannel& channel);
    vec3 convertVector(const aiVector3D& vec) const;
    quat convertQuaternion(const aiQuaternion& quat) const;

    // Keyframe extraction helpers
    void extractPositionKeys(const aiNodeAnim* nodeAnim, vector<PositionKey>& keys);
    void extractRotationKeys(const aiNodeAnim* nodeAnim, vector<RotationKey>& keys);
    void extractScaleKeys(const aiNodeAnim* nodeAnim, vector<ScaleKey>& keys);

    // Helper methods for scene graph
    unique_ptr<SceneNode> buildSceneNode(const aiNode* aiNode, SceneNode* parent);
    void traverseNodeHierarchy(SceneNode* node, const mat4& parentTransform,
                               vector<mat4>& boneTransforms);
};

} // namespace hlab