#pragma once

#include "Context.h"
#include "Image2D.h"
#include "DescriptorSet.h"
#include "ResourceBinding.h"
#include "VulkanTools.h"
#include "Sampler.h"
#include <vector>
#include <queue>
#include <string>

namespace hlab {

using namespace std;

class TextureManager
{
    friend class Model;

  public:
    TextureManager(Context& ctx);
    TextureManager(TextureManager&& other) noexcept;
    ~TextureManager();
    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(TextureManager&& other) = delete;
    TextureManager& operator=(const TextureManager&) = delete;

    auto resourceBinding() -> ResourceBinding&
    {
        return bindlessResourceBinding_;
    }

  private:
    const uint32_t kMaxTextures_ = 512;

    Context& ctx_;

    vector<Image2D> textures_;
    Image2D dummyTexture_;
    Sampler samplerLinearRepeat_;

    ResourceBinding bindlessResourceBinding_;
};

} // namespace hlab
