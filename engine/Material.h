#pragma once

#include "Context.h"
#include "Sampler.h"
#include "Image2D.h"
#include "ResourceBinding.h"
#include "DescriptorSet.h"
#include "UniformBuffer.h"
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vulkan/vulkan.h>

using namespace glm;
using namespace std;

namespace hlab {

struct MaterialUBO // TODO: this structure may need be included in class Material
{
    alignas(16) vec4 emissiveFactor_ = vec4(0.0f);
    alignas(16) vec4 baseColorFactor_ = vec4(1.0f);
    alignas(4) float roughness_ = 1.0f;
    alignas(4) float transparencyFactor_ = 1.0f;
    alignas(4) float discardAlpha_ = 0.0f;
    alignas(4) float metallicFactor_ = 0.0f;
    alignas(4) int baseColorTextureIndex_ = -1;
    alignas(4) int emissiveTextureIndex_ = -1;
    alignas(4) int normalTextureIndex_ = -1;
    alignas(4) int opacityTextureIndex_ = -1;
    alignas(4) int metallicRoughnessTextureIndex_ = -1;
    alignas(4) int occlusionTextureIndex_ = -1;
};

class Material
{
  public:
    Material()
    {
        ubo_.emissiveFactor_ = vec4(0.0f, 0.0f, 0.0f, 0.0f);
        ubo_.baseColorFactor_ = vec4(1.0f, 1.0f, 1.0f, 1.0f);
        ubo_.roughness_ = 1.0f;
        ubo_.transparencyFactor_ = 1.0f;
        ubo_.discardAlpha_ = 0.0f;
        ubo_.metallicFactor_ = 0.0f;
        // TODO: maybe add occlusionStrength and normalScale

        // 텍스쳐 배열에 대한 인덱스
        ubo_.baseColorTextureIndex_ = -1;
        ubo_.emissiveTextureIndex_ = -1;
        ubo_.normalTextureIndex_ = -1;
        ubo_.opacityTextureIndex_ = -1;
        ubo_.metallicRoughnessTextureIndex_ = -1;
        ubo_.occlusionTextureIndex_ = -1;
        // ubo_.flags_ = sCastShadow | sReceiveShadow;
    }

    enum Flags {
        sCastShadow = 0x1,
        sReceiveShadow = 0x2,
        sTransparent = 0x4,
    };

    MaterialUBO ubo_;
    uint32_t flags_ = sCastShadow | sReceiveShadow;

    string name_;

    void loadFromCache(const string& cachePath);
    void writeToCache(const string& cachePath);

  private:
};

} // namespace hlab
