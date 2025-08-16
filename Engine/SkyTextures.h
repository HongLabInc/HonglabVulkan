#pragma once

#include "Context.h"
#include "Image2D.h"
#include "Sampler.h"
#include <string>
#include <vulkan/vulkan.h>

namespace hlab {

class SkyTextures
{
  public:
    SkyTextures(Context& ctx);
    ~SkyTextures();

    // Load all IBL textures from specified file names
    void loadKtxMaps(const std::string& prefilteredFilename, const std::string& irradianceFilename,
                     const std::string& brdfLutFileName);

    Image2D& prefiltered()
    {
        return prefiltered_;
    }
    Image2D& irradiance()
    {
        return irradiance_;
    }
    Image2D& brdfLUT()
    {
        return brdfLUT_;
    }

    void cleanup();

    void createDescriptorSet()
    {
    }

  private:
    Context& ctx_;

    // IBL texture resources
    Image2D prefiltered_; // Prefiltered environment map for specular
    Image2D irradiance_;  // Convolved irradiance cubemap for diffuse
    Image2D brdfLUT_;     // BRDF integration lookup texture

    Sampler samplerLinearRepeat_;
    Sampler samplerLinearClamp_;

    VkDescriptorSetLayout descriptorSetLayout_{VK_NULL_HANDLE};
    VkDescriptorSet descriptorSet_{VK_NULL_HANDLE};
};

} // namespace hlab