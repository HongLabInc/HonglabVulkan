#include "TextureManager.h"
#include "Logger.h"

namespace hlab {

TextureManager::TextureManager(Context& ctx)
    : ctx_(ctx), dummyTexture_(ctx), samplerLinearRepeat_(ctx)
{
    samplerLinearRepeat_.createLinearRepeat();

    uint8_t dummyColor[4] = {255, 255, 0, 255};
    dummyTexture_.createSolid(2, 2, dummyColor);
    dummyTexture_.setSampler(samplerLinearRepeat_.handle());

    // 주의: 현재는 더미 텍스쳐 하나만 넣어보고 있습니다.
    bindlessResourceBinding_.descriptorType_ = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindlessResourceBinding_.descriptorCount_ = kMaxTextures_;
    bindlessResourceBinding_.image_ = dummyTexture_.image();
    bindlessResourceBinding_.imageView_ = dummyTexture_.view();
    bindlessResourceBinding_.sampler_ = samplerLinearRepeat_.handle();
    bindlessResourceBinding_.update();
}

TextureManager::~TextureManager()
{
}

TextureManager::TextureManager(TextureManager&& other) noexcept
    : ctx_(other.ctx_), dummyTexture_(std::move(other.dummyTexture_)),
      samplerLinearRepeat_(std::move(other.samplerLinearRepeat_)),
      bindlessResourceBinding_(std::move(other.bindlessResourceBinding_))
{
    // No additional cleanup needed for the moved-from object
    // since all members have been properly moved
}

} // namespace hlab