#pragma once

#include <vulkan/vulkan.h>

namespace hlab {

class Context; // Forward declaration

class Sampler
{
  public:
    Sampler(Context& ctx);

    ~Sampler();

    auto handle() const -> VkSampler;

    void createAnisoRepeat();
    void createAnisoClamp();
    void createLinearRepeat();
    void createLinearClamp();

    void cleanup();

  private:
    Context& ctx_;
    VkSampler sampler_{VK_NULL_HANDLE};
};

} // namespace hlab