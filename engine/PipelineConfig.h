#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <optional>

namespace hlab {

// Core pipeline configuration structure
struct PipelineConfig
{
    // Pipeline type and identification
    std::string name;
    enum class Type { Graphics, Compute } type = Type::Graphics;

    // Required format parameters (empty = not required)
    struct RequiredFormats
    {
        bool outColorFormat = false;
        bool depthFormat = false;
        bool msaaSamples = false;
    } requiredFormats;

    // Vertex input configuration
    struct VertexInput
    {
        enum class Type { None, ImGui, Standard } type = Type::None;
        // None: No vertex input (shader-generated geometry)
        // ImGui: Custom ImGui vertex format
        // Standard: Standard 3D vertex with Vertex::getAttributeDescriptions()
    } vertexInput;

    // Depth/Stencil configuration
    struct DepthStencil
    {
        bool depthTest = false;
        bool depthWrite = false;
        VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    } depthStencil;

    // Rasterization settings (only non-default values)
    struct Rasterization
    {
        VkCullModeFlags cullMode = VK_CULL_MODE_NONE;
        VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        // Shadow-specific settings
        bool depthClampEnable = false;
        bool depthBiasEnable = false;
        float depthBiasConstantFactor = 0.0f;
        float depthBiasSlopeFactor = 0.0f;
    } rasterization;

    // Color blending configuration
    struct ColorBlend
    {
        bool blendEnable = false;

        // Alpha blending preset (for GUI)
        struct AlphaBlending
        {
            VkBlendFactor srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            VkBlendFactor dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            VkBlendFactor srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            VkBlendFactor dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        } alphaBlending;
    } colorBlend;

    // Multisampling configuration
    struct Multisample
    {
        enum class Type { Single, Variable } type = Type::Single;
        // Single: VK_SAMPLE_COUNT_1_BIT
        // Variable: Uses msaaSamples parameter
    } multisample;

    // Dynamic state configuration
    struct DynamicState
    {
        std::vector<VkDynamicState> states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        // Shadow maps add VK_DYNAMIC_STATE_DEPTH_BIAS
    } dynamicState;

    // Pipeline-specific configurations
    struct SpecialConfig
    {
        bool isDepthOnly = false;           // ShadowMap: no color attachments
        bool isScreenSpace = false;         // Post/Sky: no vertex buffers
        bool hasCustomVertexFormat = false; // GUI: ImGui vertex format
    } specialConfig;

    // Factory methods for common configurations
    static PipelineConfig createGui();
    static PipelineConfig createPbrForward();
    static PipelineConfig createPost();
    static PipelineConfig createShadowMap();
    static PipelineConfig createSky();
    static PipelineConfig createCompute();
    static PipelineConfig createSsao();
    static PipelineConfig createTriangle();
};

// Implementation of factory methods
inline PipelineConfig PipelineConfig::createGui()
{
    PipelineConfig config;
    config.name = "gui";
    config.requiredFormats.outColorFormat = true;
    config.vertexInput.type = VertexInput::Type::ImGui;
    config.colorBlend.blendEnable = true;
    config.specialConfig.hasCustomVertexFormat = true;
    return config;
}

inline PipelineConfig PipelineConfig::createPbrForward()
{
    PipelineConfig config;
    config.name = "pbrForward";
    config.requiredFormats = {true, true, true}; // color, depth, msaa
    config.vertexInput.type = VertexInput::Type::Standard;
    config.depthStencil = {true, true, VK_COMPARE_OP_LESS_OR_EQUAL};
    config.multisample.type = Multisample::Type::Variable;
    config.rasterization.frontFace = VK_FRONT_FACE_CLOCKWISE;
    return config;
}

inline PipelineConfig PipelineConfig::createPost()
{
    PipelineConfig config;
    config.name = "post";
    config.requiredFormats = {true, true, false}; // color, depth
    config.specialConfig.isScreenSpace = true;
    return config;
}

inline PipelineConfig PipelineConfig::createShadowMap()
{
    PipelineConfig config;
    config.name = "shadowMap";
    config.vertexInput.type = VertexInput::Type::Standard;
    config.depthStencil = {true, true, VK_COMPARE_OP_LESS};
    config.rasterization = {.cullMode = VK_CULL_MODE_NONE,
                            .frontFace = VK_FRONT_FACE_CLOCKWISE,
                            .depthClampEnable = true,
                            .depthBiasEnable = true,
                            .depthBiasConstantFactor = 1.1f,
                            .depthBiasSlopeFactor = 2.0f};
    config.dynamicState.states.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS);
    config.specialConfig.isDepthOnly = true;
    return config;
}

inline PipelineConfig PipelineConfig::createSky()
{
    PipelineConfig config;
    config.name = "sky";
    config.requiredFormats = {true, true, true};                      // color, depth, msaa
    config.depthStencil = {true, false, VK_COMPARE_OP_LESS_OR_EQUAL}; // test but no write
    config.multisample.type = Multisample::Type::Variable;
    config.specialConfig.isScreenSpace = true;
    return config;
}

inline PipelineConfig PipelineConfig::createCompute()
{
    PipelineConfig config;
    config.name = "compute";
    config.type = Type::Compute;
    return config;
}

inline PipelineConfig PipelineConfig::createSsao()
{
    PipelineConfig config;
    config.name = "ssao";
    config.type = Type::Compute;
    return config;
}

inline PipelineConfig PipelineConfig::createTriangle()
{
    PipelineConfig config;
    config.name = "triangle";
    config.requiredFormats.outColorFormat = true;
    config.specialConfig.isScreenSpace = true;
    return config;
}

} // namespace hlab