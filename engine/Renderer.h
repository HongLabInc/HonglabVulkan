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
#include "TextureManager.h"
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

struct SkyOptionsUBO
{
    float environmentIntensity = 1.0f;
    float roughnessLevel = 0.5f;
    uint32_t useIrradianceMap = 0;

    uint32_t showMipLevels = 0;
    uint32_t showCubeFaces = 0;
    float padding1;
    float padding2;
    float padding3;
};

struct OptionsUniform
{
    alignas(4) int textureOn = 1;   // Use int instead of bool, 1 = true, 0 = false
    alignas(4) int shadowOn = 1;    // Use int instead of bool, 1 = true, 0 = false
    alignas(4) int discardOn = 1;   // Use int instead of bool, 1 = true, 0 = false
    alignas(4) int animationOn = 1; // Use int instead of bool, 1 = true, 0 = false
};

// Post-processing options uniform buffer structure
struct PostOptionsUBO
{
    // Basic tone mapping
    alignas(4) int toneMappingType = 2; // 0=None, 1=Reinhard, 2=ACES, etc.
    alignas(4) float exposure = 1.0f;   // Exposure adjustment
    alignas(4) float gamma = 2.2f;      // Gamma correction
    alignas(4) float maxWhite = 11.2f;  // For Reinhard extended

    // Color grading
    alignas(4) float contrast = 1.0f;   // Contrast adjustment
    alignas(4) float brightness = 0.0f; // Brightness adjustment
    alignas(4) float saturation = 1.0f; // Saturation adjustment
    alignas(4) float vibrance = 0.0f;   // Vibrance adjustment

    // Effects
    alignas(4) float vignetteStrength = 0.0f;    // Vignette effect strength
    alignas(4) float vignetteRadius = 0.8f;      // Vignette radius
    alignas(4) float filmGrainStrength = 0.0f;   // Film grain effect
    alignas(4) float chromaticAberration = 0.0f; // Chromatic aberration

    // Debug options
    alignas(4) int debugMode = 0;       // Debug visualization mode
    alignas(4) int showOnlyChannel = 0; // Show specific color channel
    alignas(4) float debugSplit = 0.5f; // Split screen position for comparison
    alignas(4) float padding1 = 0.0f;   // Alignment padding
};

struct SsaoOptionsUBO
{
    alignas(4) float ssaoRadius = 0.1f;
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
    void performFrustumCulling(vector<Model>& models); // Overload for all models
    void updateWorldBounds(vector<Model>& models);     // Update world bounds for all models
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
    auto skyOptionsUBO() -> SkyOptionsUBO&
    {
        return skyOptionsUBO_;
    }
    auto postOptionsUBO() -> PostOptionsUBO&
    {
        return postOptionsUBO_;
    }
    auto ssaoOptionsUBO() -> SsaoOptionsUBO&
    {
        return ssaoOptionsUBO_;
    }

  private:
    const uint32_t& kMaxFramesInFlight_; // 2;
    const string& kAssetsPathPrefix_;    // "../../assets/";
    const string& kShaderPathPrefix_;    // kAssetsPathPrefix + "shaders/";

    Context& ctx_;
    ShaderManager& shaderManager_;

    TextureManager textureManager_;

    // Per frame uniform buffers
    SceneUniform sceneUBO_{};
    SkyOptionsUBO skyOptionsUBO_{};
    OptionsUniform optionsUBO_{};
    BoneDataUniform boneDataUBO_{};
    PostOptionsUBO postOptionsUBO_{};
    SsaoOptionsUBO ssaoOptionsUBO_{};

    vector<UniformBuffer<SceneUniform>> sceneUniforms_{};
    vector<UniformBuffer<SkyOptionsUBO>> skyOptionsUniforms_;
    vector<UniformBuffer<OptionsUniform>> optionsUniforms_{};
    vector<UniformBuffer<BoneDataUniform>> boneDataUniforms_;
    vector<UniformBuffer<PostOptionsUBO>> postOptionsUniforms_;
    vector<UniformBuffer<SsaoOptionsUBO>> ssaoOptionsUniforms_;

    vector<DescriptorSet> sceneOptionsBoneDataSets_{};
    vector<DescriptorSet> sceneSkyOptionsSets_{};
    vector<DescriptorSet> postProcessingDescriptorSets_;
    vector<DescriptorSet> ssaoDescriptorSets_;

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