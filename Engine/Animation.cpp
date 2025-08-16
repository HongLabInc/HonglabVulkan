#include "Animation.h"
#include "Logger.h"
#include <algorithm>
#include <functional>
#include <glm/gtx/matrix_interpolation.hpp>
#include <glm/gtc/type_ptr.hpp> // Required for glm::make_mat4

namespace hlab {

Animation::Animation()
    : currentAnimationIndex_(0), currentTime_(0.0f), playbackSpeed_(1.0f), isPlaying_(false),
      isLooping_(true), globalInverseTransform_(1.0f)
{
}

Animation::~Animation() = default;

void Animation::loadFromScene(const aiScene* scene)
{
    if (!scene) {
        printLog("Animation::loadFromScene - Invalid scene");
        return;
    }

    printLog("Loading animation data from scene...");
    printLog("  Animations found: {}", scene->mNumAnimations);

    // Store global inverse transform
    if (scene->mRootNode) {
        globalInverseTransform_ =
            glm::inverse(glm::transpose(glm::make_mat4(&scene->mRootNode->mTransformation.a1)));
    }

    buildSceneGraph(scene);
    processBones(scene);
    buildBoneHierarchy(scene);
    assignGlobalBoneIds();

    if (scene->mNumAnimations > 0) {
        processAnimations(scene);
    }

    // Initialize bone matrices
    boneMatrices_.resize(bones_.size(), mat4(1.0f));

    printLog("Animation loading complete:");
    printLog("  Animation clips: {}", animations_.size());
    printLog("  Bones: {}", bones_.size());
    printLog("  Scene nodes: {}", nodeMapping_.size());
}

void Animation::processBones(const aiScene* scene)
{
    if (!scene)
        return;

    printLog("Processing bones for global hierarchy...");

    // Collect all unique bone names from all meshes
    unordered_map<string, mat4> boneOffsetMatrices;
    unordered_map<string, vector<Bone::VertexWeight>> boneWeights;

    uint32_t totalMeshBones = 0;
    for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        const aiMesh* mesh = scene->mMeshes[meshIndex];

        if (!mesh->HasBones())
            continue;

        // printLog("  Processing {} bones from mesh '{}'", mesh->mNumBones, mesh->mName.C_Str());

        totalMeshBones += mesh->mNumBones;

        for (uint32_t boneIdx = 0; boneIdx < mesh->mNumBones; ++boneIdx) {
            const aiBone* aiBone = mesh->mBones[boneIdx];
            string boneName = aiBone->mName.C_Str();

            // Store offset matrix (should be same for all meshes)
            if (boneOffsetMatrices.find(boneName) == boneOffsetMatrices.end()) {
                boneOffsetMatrices[boneName] =
                    glm::transpose(glm::make_mat4(&aiBone->mOffsetMatrix.a1));
            }

            // Collect vertex weights
            for (uint32_t weightIdx = 0; weightIdx < aiBone->mNumWeights; ++weightIdx) {
                const aiVertexWeight& weight = aiBone->mWeights[weightIdx];
                boneWeights[boneName].push_back({weight.mVertexId, weight.mWeight});
            }
        }
    }

    // Create global bone list with proper IDs
    bones_.clear();
    boneMapping_.clear();
    globalBoneNameToId_.clear();

    uint32_t globalBoneIndex = 0;
    for (const auto& pair : boneOffsetMatrices) {
        const string& boneName = pair.first;
        const mat4& offsetMatrix = pair.second;

        Bone bone;
        bone.name = boneName;
        bone.id = globalBoneIndex;
        bone.offsetMatrix = offsetMatrix;
        bone.weights = boneWeights[boneName];

        bones_.push_back(bone);
        boneMapping_[boneName] = globalBoneIndex;
        globalBoneNameToId_[boneName] = globalBoneIndex;

        globalBoneIndex++;
    }

    printLog("Created {} global bones from {} total mesh bones", bones_.size(), totalMeshBones);

    // After collecting weights, verify they sum to 1.0
    for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        const aiMesh* mesh = scene->mMeshes[meshIndex];
        if (!mesh->HasBones())
            continue;

        // Track total weight per vertex
        vector<float> vertexWeightSums(mesh->mNumVertices, 0.0f);

        for (uint32_t boneIdx = 0; boneIdx < mesh->mNumBones; ++boneIdx) {
            const aiBone* aiBone = mesh->mBones[boneIdx];
            for (uint32_t weightIdx = 0; weightIdx < aiBone->mNumWeights; ++weightIdx) {
                const aiVertexWeight& weight = aiBone->mWeights[weightIdx];
                vertexWeightSums[weight.mVertexId] += weight.mWeight;
            }
        }

        // Verify weights sum to 1.0 (with epsilon tolerance)
        for (uint32_t i = 0; i < mesh->mNumVertices; ++i) {
            if (vertexWeightSums[i] > 0.0f && std::abs(vertexWeightSums[i] - 1.0f) > 0.01f) {
                printLog("WARNING: Vertex {} in mesh '{}' has total weight {:.3f} (expected 1.0)",
                         i, mesh->mName.C_Str(), vertexWeightSums[i]);
            }
        }
    }
}

void Animation::buildBoneHierarchy(const aiScene* scene)
{
    if (!scene || !scene->mRootNode)
        return;

    // printLog("Building bone hierarchy...");

    // Reset parent indices
    for (auto& bone : bones_) {
        bone.parentIndex = -1;
    }

    // Function to find parent bone recursively
    std::function<const aiNode*(const aiNode*)> findBoneParent =
        [&](const aiNode* node) -> const aiNode* {
        if (!node || !node->mParent)
            return nullptr;

        if (globalBoneNameToId_.find(node->mParent->mName.C_Str()) != globalBoneNameToId_.end()) {
            return node->mParent;
        }

        return findBoneParent(node->mParent);
    };

    // Function to traverse scene nodes and establish hierarchy
    std::function<void(const aiNode*)> traverseNodes = [&](const aiNode* node) {
        if (!node)
            return;

        string nodeName = node->mName.C_Str();

        // If this node represents a bone
        if (globalBoneNameToId_.find(nodeName) != globalBoneNameToId_.end()) {
            int boneIndex = globalBoneNameToId_[nodeName];

            // Find parent bone
            const aiNode* parentBone = findBoneParent(node);
            if (parentBone) {
                string parentName = parentBone->mName.C_Str();
                if (globalBoneNameToId_.find(parentName) != globalBoneNameToId_.end()) {
                    int parentIndex = globalBoneNameToId_[parentName];
                    bones_[boneIndex].parentIndex = parentIndex;
                    // printLog("  Bone '{}' [{}] -> parent '{}' [{}]", nodeName, boneIndex,
                    //          parentName, parentIndex);
                }
            }
        }

        // Process children
        for (uint32_t i = 0; i < node->mNumChildren; ++i) {
            traverseNodes(node->mChildren[i]);
        }
    };

    traverseNodes(scene->mRootNode);
    // printLog("Bone hierarchy established");
}

void Animation::assignGlobalBoneIds()
{
    // Create ID to name mapping
    globalBoneIdToName_.resize(bones_.size());
    for (const auto& pair : globalBoneNameToId_) {
        globalBoneIdToName_[pair.second] = pair.first;
    }

    printLog("Global bone ID assignment complete: {} bones", bones_.size());
}

int Animation::getGlobalBoneIndex(const string& boneName) const
{
    auto it = globalBoneNameToId_.find(boneName);
    return (it != globalBoneNameToId_.end()) ? it->second : -1;
}

void Animation::processAnimations(const aiScene* scene)
{
    animations_.reserve(scene->mNumAnimations);

    for (uint32_t i = 0; i < scene->mNumAnimations; ++i) {
        const aiAnimation* aiAnim = scene->mAnimations[i];

        AnimationClip clip;
        clip.name = aiAnim->mName.C_Str();
        clip.duration = aiAnim->mDuration;
        clip.ticksPerSecond = aiAnim->mTicksPerSecond != 0 ? aiAnim->mTicksPerSecond : 25.0;

        // printLog("Processing animation '{}' - Duration: {:.2f}s, FPS: {:.1f}", clip.name,
        //          clip.duration / clip.ticksPerSecond, clip.ticksPerSecond);

        // Process animation channels
        clip.channels.reserve(aiAnim->mNumChannels);
        for (uint32_t j = 0; j < aiAnim->mNumChannels; ++j) {
            const aiNodeAnim* nodeAnim = aiAnim->mChannels[j];

            AnimationChannel channel;
            processAnimationChannel(nodeAnim, channel);
            clip.channels.push_back(std::move(channel));
        }

        animations_.push_back(std::move(clip));
    }
}

void Animation::processAnimationChannel(const aiNodeAnim* nodeAnim, AnimationChannel& channel)
{
    channel.nodeName = nodeAnim->mNodeName.C_Str();

    // Extract position keys
    extractPositionKeys(nodeAnim, channel.positionKeys);

    // Extract rotation keys
    extractRotationKeys(nodeAnim, channel.rotationKeys);

    // Extract scale keys
    extractScaleKeys(nodeAnim, channel.scaleKeys);

    // printLog("  Channel '{}': {} pos, {} rot, {} scale keys", channel.nodeName,
    //          channel.positionKeys.size(), channel.rotationKeys.size(), channel.scaleKeys.size());
}

void Animation::extractPositionKeys(const aiNodeAnim* nodeAnim, vector<PositionKey>& keys)
{
    keys.reserve(nodeAnim->mNumPositionKeys);
    for (uint32_t i = 0; i < nodeAnim->mNumPositionKeys; ++i) {
        const aiVectorKey& key = nodeAnim->mPositionKeys[i];
        keys.emplace_back(key.mTime, vec3(key.mValue.x, key.mValue.y, key.mValue.z));
    }
}

void Animation::extractRotationKeys(const aiNodeAnim* nodeAnim, vector<RotationKey>& keys)
{
    keys.reserve(nodeAnim->mNumRotationKeys);
    for (uint32_t i = 0; i < nodeAnim->mNumRotationKeys; ++i) {
        const aiQuatKey& key = nodeAnim->mRotationKeys[i];
        keys.emplace_back(key.mTime, quat(key.mValue.w, key.mValue.x, key.mValue.y, key.mValue.z));
    }
}

void Animation::extractScaleKeys(const aiNodeAnim* nodeAnim, vector<ScaleKey>& keys)
{
    keys.reserve(nodeAnim->mNumScalingKeys);
    for (uint32_t i = 0; i < nodeAnim->mNumScalingKeys; ++i) {
        const aiVectorKey& key = nodeAnim->mScalingKeys[i];
        keys.emplace_back(key.mTime, vec3(key.mValue.x, key.mValue.y, key.mValue.z));
    }
}

void Animation::updateAnimation(float deltaTime)
{
    if (!isPlaying_ || animations_.empty())
        return;

    currentTime_ += deltaTime * playbackSpeed_;

    const AnimationClip& currentAnim = animations_[currentAnimationIndex_];
    double animationTime = currentTime_ * currentAnim.ticksPerSecond;

    // Handle looping
    if (isLooping_ && animationTime > currentAnim.duration) {
        currentTime_ = 0.0f;
        animationTime = 0.0;
    } else if (!isLooping_ && animationTime > currentAnim.duration) {
        currentTime_ = static_cast<float>(currentAnim.duration / currentAnim.ticksPerSecond);
        animationTime = currentAnim.duration;
        isPlaying_ = false;
    }

    // Update bone transformations using proper hierarchy
    if (rootNode_) {
        calculateBoneTransforms(boneMatrices_);
    }
}

void Animation::calculateBoneTransforms(vector<mat4>& transforms, const string& nodeName,
                                        const mat4& parentTransform)
{
    if (animations_.empty() || !rootNode_) {
        return;
    }

    // Start hierarchical traversal from root
    traverseNodeHierarchy(rootNode_.get(), mat4(1.0f), transforms);
}

void Animation::traverseNodeHierarchy(SceneNode* node, const mat4& parentTransform,
                                      vector<mat4>& boneTransforms)
{
    if (!node)
        return;

    const AnimationClip& currentAnim = animations_[currentAnimationIndex_];
    double animationTime = currentTime_ * currentAnim.ticksPerSecond;

    // Get animated transformation for this node
    mat4 nodeTransformation = getNodeTransformation(node->name, animationTime);

    // If no animation channel exists, use the original transformation
    if (nodeTransformation == mat4(1.0f)) {
        nodeTransformation = node->transformation;
    }

    // Calculate global transformation
    mat4 globalTransformation = parentTransform * nodeTransformation;

    // Check if this node is a bone
    auto it = globalBoneNameToId_.find(node->name);
    if (it != globalBoneNameToId_.end()) {
        uint32_t boneIndex = it->second;
        if (boneIndex < bones_.size() && boneIndex < boneTransforms.size()) {
            // FIXED: Correct bone transformation calculation
            // The proper formula is: FinalTransform = GlobalInverse * GlobalTransform *
            // OffsetMatrix This transforms: vertex -> bone space (offset) -> world space (global)
            // -> root space (inverse)
            boneTransforms[boneIndex] =
                globalInverseTransform_ * globalTransformation * bones_[boneIndex].offsetMatrix;
        }
    }

    // Recursively process children
    for (const auto& child : node->children) {
        traverseNodeHierarchy(child.get(), globalTransformation, boneTransforms);
    }
}

mat4 Animation::getNodeTransformation(const string& nodeName, double time) const
{
    if (animations_.empty())
        return mat4(1.0f);

    const AnimationClip& currentAnim = animations_[currentAnimationIndex_];

    // Find animation channel for this node
    for (const auto& channel : currentAnim.channels) {
        if (channel.nodeName == nodeName) {
            vec3 position = channel.interpolatePosition(time);
            quat rotation = channel.interpolateRotation(time);
            vec3 scale = channel.interpolateScale(time);

            mat4 translation = glm::translate(mat4(1.0f), position);
            mat4 rotationMat = glm::mat4_cast(rotation);
            mat4 scaleMat = glm::scale(mat4(1.0f), scale);

            return translation * rotationMat * scaleMat;
        }
    }

    return mat4(1.0f);
}

// Add this new method to build the scene graph
void Animation::buildSceneGraph(const aiScene* scene)
{
    if (!scene || !scene->mRootNode)
        return;

    printLog("Building animation scene graph...");
    nodeMapping_.clear();
    rootNode_ = buildSceneNode(scene->mRootNode, nullptr);
    printLog("Scene graph built with {} nodes", nodeMapping_.size());
}

unique_ptr<Animation::SceneNode> Animation::buildSceneNode(const aiNode* aiNode, SceneNode* parent)
{
    auto node = make_unique<SceneNode>(aiNode->mName.C_Str());
    node->transformation = glm::transpose(glm::make_mat4(&aiNode->mTransformation.a1));
    node->parent = parent;

    // Add to quick lookup map
    nodeMapping_[node->name] = node.get();

    // Process children
    for (uint32_t i = 0; i < aiNode->mNumChildren; ++i) {
        auto child = buildSceneNode(aiNode->mChildren[i], node.get());
        node->children.push_back(std::move(child));
    }

    return node;
}

// AnimationChannel interpolation methods
vec3 AnimationChannel::interpolatePosition(double time) const
{
    return interpolateKeys(positionKeys, time);
}

quat AnimationChannel::interpolateRotation(double time) const
{
    if (rotationKeys.empty())
        return quat(1.0f, 0.0f, 0.0f, 0.0f);
    if (rotationKeys.size() == 1)
        return rotationKeys[0].value;

    // Find surrounding keyframes
    uint32_t index = 0;
    for (uint32_t i = 0; i < rotationKeys.size() - 1; ++i) {
        if (time < rotationKeys[i + 1].time) {
            index = i;
            break;
        }
    }

    if (index >= rotationKeys.size() - 1) {
        return rotationKeys.back().value;
    }

    const auto& key1 = rotationKeys[index];
    const auto& key2 = rotationKeys[index + 1];

    double deltaTime = key2.time - key1.time;
    float factor = static_cast<float>((time - key1.time) / deltaTime);

    return glm::slerp(key1.value, key2.value, factor);
}

vec3 AnimationChannel::interpolateScale(double time) const
{
    return interpolateKeys(scaleKeys, time);
}

template <typename T>
T AnimationChannel::interpolateKeys(const vector<AnimationKey<T>>& keys, double time) const
{
    if (keys.empty())
        return T{};
    if (keys.size() == 1)
        return keys[0].value;

    // Find surrounding keyframes
    uint32_t index = 0;
    for (uint32_t i = 0; i < keys.size() - 1; ++i) {
        if (time < keys[i + 1].time) {
            index = i;
            break;
        }
    }

    if (index >= keys.size() - 1) {
        return keys.back().value;
    }

    const auto& key1 = keys[index];
    const auto& key2 = keys[index + 1];

    double deltaTime = key2.time - key1.time;
    float factor = static_cast<float>((time - key1.time) / deltaTime);

    return glm::mix(key1.value, key2.value, factor);
}

// Getters
float Animation::getDuration() const
{
    if (animations_.empty())
        return 0.0f;
    const auto& currentAnim = animations_[currentAnimationIndex_];
    return static_cast<float>(currentAnim.duration / currentAnim.ticksPerSecond);
}

const string& Animation::getCurrentAnimationName() const
{
    static const string empty = "";
    if (animations_.empty())
        return empty;
    return animations_[currentAnimationIndex_].name;
}

void Animation::setAnimationIndex(uint32_t index)
{
    if (index < animations_.size()) {
        currentAnimationIndex_ = index;
        currentTime_ = 0.0f;
    }
}

void Animation::setPlaybackSpeed(float speed)
{
    playbackSpeed_ = speed;
}

void Animation::setLooping(bool loop)
{
    isLooping_ = loop;
}

} // namespace hlab
