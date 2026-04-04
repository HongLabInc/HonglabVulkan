#pragma once

#include "Context.h"

#include <unordered_map>
#include <vulkan/vulkan.h>
#include <array>

namespace hlab {

using namespace std;

class Shader
{
    friend class ShaderManager; // Allow ShaderManager to access private members

  public:
    Shader(Context& ctx, string spvFilename);

    Shader(Shader&& other) noexcept;

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader& operator=(Shader&&) = delete;

    ~Shader();

    void cleanup();

  private:
    Context& ctx_; // for creation and cleanup
    VkShaderModule shaderModule_{VK_NULL_HANDLE};
    VkShaderStageFlagBits stage_{VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM};
    string name_{""};

    auto readSpvFile(const string& spvFilename) -> vector<char>;
    auto createShaderModule(const vector<char>& shaderCode) -> VkShaderModule;
    static auto stageFromFilename(const string& filename) -> VkShaderStageFlagBits;
};

} // namespace hlab
