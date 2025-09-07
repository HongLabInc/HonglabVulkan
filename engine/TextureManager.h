#pragma once

#include "Context.h"
#include "Image2D.h"
#include "DescriptorSet.h"
#include "VulkanTools.h"
#include <vector>
#include <queue>
#include <string>

namespace hlab {

using namespace std;

class TextureManager
{
  public:
    TextureManager(Context& ctx);
    ~TextureManager();

    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;

  private:
    const uint32_t kMaxTextures_ = 512;

    Context& ctx_;
};

} // namespace hlab
