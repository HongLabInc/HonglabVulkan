#include "TextureManager.h"
#include "Logger.h"

namespace hlab {

TextureManager::TextureManager(Context& ctx) : Resource(ctx, Resource::Type::Buffer)
{
}

TextureManager::~TextureManager()
{
}

TextureManager::TextureManager(TextureManager&& other) noexcept
    : Resource(std::move(other)), textures_(std::move(other.textures_))
{
    // No additional cleanup needed for the moved-from object
    // since all members have been properly moved
}

void TextureManager::cleanup()
{
    for (auto& texture : textures_) {
        if (texture) {
            texture->cleanup();
        }
    }
    textures_.clear();
}

} // namespace hlab