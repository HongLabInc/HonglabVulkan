#include "SkyTextures.h"
#include "Context.h"
#include "VulkanTools.h"
#include "Logger.h"

namespace hlab {

SkyTextures::SkyTextures(Context& ctx)
    : ctx_(ctx), prefiltered_(ctx), irradiance_(ctx), brdfLUT_(ctx), samplerLinearRepeat_(ctx),
      samplerLinearClamp_(ctx)
{
    samplerLinearRepeat_.createLinearRepeat();
    samplerLinearClamp_.createLinearClamp();
}

SkyTextures::~SkyTextures()
{
    cleanup();
}

void SkyTextures::loadKtxMaps(const std::string& prefilteredFilename,
                              const std::string& irradianceFilename,
                              const std::string& brdfLutFileName)
{
    printLog("Loading IBL textures...");
    printLog("  Prefiltered: {}", prefilteredFilename);
    printLog("  Irradiance: {}", irradianceFilename);
    printLog("  BRDF LUT: {}", brdfLutFileName);

    // Load prefiltered environment map (cubemap for specular reflections)
    prefiltered_.createTextureFromKtx2(prefilteredFilename, true);
    prefiltered_.setSampler(samplerLinearRepeat_.handle());

    // Load irradiance map (cubemap for diffuse lighting)
    irradiance_.createTextureFromKtx2(irradianceFilename, true);
    irradiance_.setSampler(samplerLinearRepeat_.handle());

    // Load BRDF lookup table (2D texture)
    brdfLUT_.createTextureFromImage(brdfLutFileName, false, false);
    brdfLUT_.setSampler(samplerLinearClamp_.handle());
}

void SkyTextures::cleanup()
{
    prefiltered_.cleanup();
    irradiance_.cleanup();
    brdfLUT_.cleanup();
}

} // namespace hlab