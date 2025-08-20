#pragma once

#include "Camera.h"
#include "DescriptorSet.h"
#include "Context.h"
#include "Image2D.h"
#include "StorageBuffer.h"
#include "Sampler.h"
#include "SkyTextures.h"
#include "Pipeline.h"
#include "DepthStencil.h"
#include "ViewFrustum.h"
#include "Model.h"
#include "UniformBuffer.h"
#include "ShaderManager.h"
#include "ShadowMap.h"
#include <glm/glm.hpp>
#include <vector>
#include <functional>

namespace hlab {

using namespace std;

struct SceneUniform // Layout matches pbrForward.vert
{
    alignas(16) glm::mat4 projection = glm::mat4(1.0f); // 64 bytes
    alignas(16) glm::mat4 view = glm::mat4(1.0f);       // 64 bytes
    alignas(16) glm::vec3 cameraPos = glm::vec3(0.0f);  // 16 bytes (vec3 + padding)
    alignas(4) float padding1 = 0.0f;                   // 4 bytes padding
    alignas(16) glm::vec3 directionalLightDir = glm::vec3(0.0f, 1.0f, 0.0f); // 16 bytes
    alignas(16) glm::vec3 directionalLightColor = glm::vec3(1.0f);
    alignas(16) glm::mat4 lightSpaceMatrix = glm::mat4(1.0f); // 64 bytes - for shadow mapping
};

struct OptionsUniform
{
    alignas(4) int textureOn = 1;   // Use int instead of bool, 1 = true, 0 = false
    alignas(4) int shadowOn = 1;    // Use int instead of bool, 1 = true, 0 = false
    alignas(4) int discardOn = 1;   // Use int instead of bool, 1 = true, 0 = false
    alignas(4) int animationOn = 1; // Use int instead of bool, 1 = true, 0 = false
    alignas(4) float ssaoRadius = 0.5f;
    alignas(4) float ssaoBias = 0.025f;
    alignas(4) int ssaoSampleCount = 16;
    alignas(4) float ssaoPower = 2.0f;
};

struct BoneDataUniform
{
    alignas(16) glm::mat4 boneMatrices[256]; // 16,384 bytes (already 16-byte aligned)
    alignas(16) glm::vec4 animationData;     // x = hasAnimation (0.0/1.0), y,z,w = future use
};

static_assert(sizeof(BoneDataUniform) % 16 == 0, "BoneDataUniform must be 16-byte aligned");
static_assert(sizeof(BoneDataUniform) == 256 * 64 + 16, "Unexpected BoneDataUniform size");

struct VulkanBuffer
{
    VkDeviceMemory memory_{VK_NULL_HANDLE};
    VkBuffer handle_{VK_NULL_HANDLE};
};

struct CullingStats
{
    uint32_t totalMeshes = 0;
    uint32_t culledMeshes = 0;
    uint32_t renderedMeshes = 0;
};

class Renderer
{
  public:
    Renderer(Context& ctx, ShaderManager& shaderManager, const uint32_t& kMaxFramesInFlight,
             const string& kAssetsPathPrefix, const string& kShaderPathPrefix_);

    ~Renderer()
    {
        cleanup();
    }

    void prepareForModels(vector<Model>& models, VkFormat outColorFormat, VkFormat depthFormat,
                          VkSampleCountFlagBits msaaSamples, uint32_t swapChainWidth,
                          uint32_t swapChainHeight);

    void createPipelines(const VkFormat colorFormat, const VkFormat depthFormat,
                         VkSampleCountFlagBits msaaSamples);
    void createTextures(uint32_t swapchainWidth, uint32_t swapchainHeight,
                        VkSampleCountFlagBits msaaSamples);
    void createUniformBuffers();

    void cleanup()
    {
        // Manual cleanup is not necessary
    }

    void update(Camera& camera, uint32_t currentFrame, double time);
    void updateBoneData(const vector<Model>& models, uint32_t currentFrame); // NEW: Add this method

    void draw(VkCommandBuffer cmd, uint32_t currentFrame, VkImageView swapchainImageView,
              vector<Model>& models, VkViewport viewport, VkRect2D scissor);

    void makeShadowMap(VkCommandBuffer cmd, uint32_t currentFrame, vector<Model>& models);

    // View frustum culling
    auto getCullingStats() const -> const CullingStats&;
    bool isFrustumCullingEnabled() const;
    void performFrustumCulling(vector<Model>& models, const glm::mat4& modelMatrix);
    void setFrustumCullingEnabled(bool enabled);
    void updateViewFrustum(const glm::mat4& viewProjection);

    auto sceneUBO() -> SceneUniform&
    {
        return sceneUBO_;
    }
    auto optionsUBO() -> OptionsUniform&
    {
        return optionsUBO_;
    }

  private:
    const uint32_t& kMaxFramesInFlight_; // 2;
    const string& kAssetsPathPrefix_;    // "../../assets/";
    const string& kShaderPathPrefix_;    // kAssetsPathPrefix + "shaders/";

    Context& ctx_;
    ShaderManager& shaderManager_;

    // Per frame uniform buffers
    SceneUniform sceneUBO_{};
    OptionsUniform optionsUBO_{};
    BoneDataUniform boneDataUBO_{};

    vector<UniformBuffer<SceneUniform>> sceneUniforms_{};
    vector<UniformBuffer<OptionsUniform>> optionsUniforms_{};
    vector<UniformBuffer<BoneDataUniform>> boneDataUniforms_; // NEW: Bone data uniforms

    vector<DescriptorSet> sceneOptionsBoneDataSets_{};

    // Resources
    Image2D msaaColorBuffer_;
    DepthStencil depthStencil_;
    DepthStencil msaaDepthStencil_;

    Image2D forwardToCompute_;
    Image2D computeToPost_;

    Image2D dummyTexture_;
    SkyTextures skyTextures_;

    Sampler samplerLinearRepeat_;
    Sampler samplerLinearClamp_;
    Sampler samplerAnisoRepeat_;
    Sampler samplerAnisoClamp_;

    ShadowMap shadowMap_;

    DescriptorSet skyDescriptorSet_;
    DescriptorSet postDescriptorSet_;
    DescriptorSet shadowMapSet_;

    unordered_map<string, Pipeline> pipelines_;

    ViewFrustum viewFrustum_{};
    bool frustumCullingEnabled_{true};

    // Statistics
    CullingStats cullingStats_;

    // Control parameters
    float directionalLightAngle1 = 27.0f;
    float directionalLightAngle2 = 3.0f;
    float directionalLightIntensity = 27.66f;

    // Shadow mapping bias parameters for real-time adjustment
    float shadowBiasConstant = 0.5f; // Constant bias factor
    float shadowBiasSlope = 1.0f;    // Slope-scaled bias factor
    float shadowBiasClamp = 0.0f;    // Bias clamp value

    // SSAO parameters
    float ssaoRadius = 0.5f;
    float ssaoBias = 0.025f;
    int ssaoSampleCount = 16;
    float ssaoPower = 2.0f;

    // Helper functions for creating rendering structures
    VkRenderingAttachmentInfo
    createColorAttachment(VkImageView imageView,
                          VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                          VkClearColorValue clearColor = {0.0f, 0.0f, 0.0f, 0.0f},
                          VkImageView resolveImageView = VK_NULL_HANDLE,
                          VkResolveModeFlagBits resolveMode = VK_RESOLVE_MODE_NONE) const;

    VkRenderingAttachmentInfo
    createDepthAttachment(VkImageView imageView,
                          VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                          float clearDepth = 1.0f, VkImageView resolveImageView = VK_NULL_HANDLE,
                          VkResolveModeFlagBits resolveMode = VK_RESOLVE_MODE_NONE) const;

    VkRenderingInfo
    createRenderingInfo(const VkRect2D& renderArea,
                        const VkRenderingAttachmentInfo* colorAttachment,
                        const VkRenderingAttachmentInfo* depthAttachment = nullptr) const;
};

} // namespace hlab