#include "Shader.h"
#include "VulkanTools.h"
#include "Logger.h"
#include <algorithm>
#include <vector>

namespace hlab {

using namespace std;

std::string extractFilename(const std::string& spvFilename)
{
    if (spvFilename.length() < 4 || spvFilename.substr(spvFilename.length() - 4) != ".spv") {
        exitWithMessage("Shader file does not have .spv extension: {}", spvFilename);
    }

    // 경로와 마지막 .spv 제거 ex: path/triangle.vert.spv -> triangle.vert
    size_t lastSlash = spvFilename.find_last_of("/\\");
    size_t start = (lastSlash == std::string::npos) ? 0 : lastSlash + 1;
    size_t end = spvFilename.length();
    size_t lastDot = spvFilename.find_last_of('.');
    if (lastDot != std::string::npos && lastDot > start)
        end = lastDot;

    return spvFilename.substr(start, end - start);
}

VkShaderStageFlagBits Shader::stageFromFilename(const string& filename)
{
    // Determine shader stage from extension before .spv
    // e.g. "shadowMap.vert.spv" -> ".vert" -> VERTEX
    //      "imgui.frag" (no .spv) -> ".frag" -> FRAGMENT
    string f = filename;

    // Strip .spv suffix if present
    if (f.length() > 4 && f.substr(f.length() - 4) == ".spv") {
        f = f.substr(0, f.length() - 4);
    }

    // Find the last dot to get the stage extension
    size_t dot = f.find_last_of('.');
    if (dot != string::npos) {
        string ext = f.substr(dot);
        if (ext == ".vert") return VK_SHADER_STAGE_VERTEX_BIT;
        if (ext == ".frag") return VK_SHADER_STAGE_FRAGMENT_BIT;
        if (ext == ".comp") return VK_SHADER_STAGE_COMPUTE_BIT;
        if (ext == ".geom") return VK_SHADER_STAGE_GEOMETRY_BIT;
        if (ext == ".tesc") return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        if (ext == ".tese") return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    }

    exitWithMessage("Cannot determine shader stage from filename: {}", filename);
    return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
}

Shader::Shader(Context& ctx, string spvFilename) : ctx_(ctx)
{
    name_ = extractFilename(spvFilename);
    stage_ = stageFromFilename(spvFilename);

    auto shaderCode = readSpvFile(spvFilename);
    shaderModule_ = createShaderModule(shaderCode);
}

Shader::Shader(Shader&& other) noexcept
    : ctx_(other.ctx_), // Copy reference (references can't be moved)
      shaderModule_(other.shaderModule_),
      stage_(other.stage_), name_(std::move(other.name_))
{
    other.shaderModule_ = VK_NULL_HANDLE;
    other.stage_ = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
}

Shader::~Shader()
{
    cleanup();
}

void Shader::cleanup()
{
    if (shaderModule_ != VK_NULL_HANDLE) {
        vkDestroyShaderModule(ctx_.device(), shaderModule_, nullptr);
        shaderModule_ = VK_NULL_HANDLE;
    }
}

vector<char> Shader::readSpvFile(const string& spvFilename)
{
    if (spvFilename.length() < 4 || spvFilename.substr(spvFilename.length() - 4) != ".spv") {
        exitWithMessage("Shader file does not have .spv extension: {}", spvFilename);
    }

    ifstream is(spvFilename, ios::binary | ios::in | ios::ate);
    if (!is.is_open()) {
        exitWithMessage("Could not open shader file: {}", spvFilename);
    }

    size_t shaderSize = static_cast<size_t>(is.tellg());
    if (shaderSize == 0 || shaderSize % 4 != 0) {
        exitWithMessage("Shader file size is invalid (must be >0 and multiple of 4): {}",
                        spvFilename);
    }
    is.seekg(0, ios::beg);

    vector<char> shaderCode(shaderSize);
    is.read(shaderCode.data(), shaderSize);
    is.close();

    return shaderCode;
}

VkShaderModule Shader::createShaderModule(const vector<char>& shaderCode)
{
    VkShaderModule shaderModule;
    VkShaderModuleCreateInfo shaderModuleCI{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    shaderModuleCI.codeSize = shaderCode.size();
    shaderModuleCI.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());
    check(vkCreateShaderModule(ctx_.device(), &shaderModuleCI, nullptr, &shaderModule));

    return shaderModule;
}

} // namespace hlab
