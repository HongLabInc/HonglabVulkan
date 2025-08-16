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

Shader::Shader(Context& ctx, string spvFilename) : ctx_(ctx)
{
    name_ = extractFilename(spvFilename);

    auto shaderCode = readSpvFile(spvFilename);
    shaderModule_ = createShaderModule(shaderCode);
    reflectModule_ = createRefModule(shaderCode);
    stage_ = static_cast<VkShaderStageFlagBits>(reflectModule_.shader_stage);
}

Shader::Shader(Shader&& other) noexcept
    : ctx_(other.ctx_), // Copy reference (references can't be moved)
      shaderModule_(other.shaderModule_), reflectModule_(other.reflectModule_),
      stage_(other.stage_), name_(std::move(other.name_))
{
    other.shaderModule_ = VK_NULL_HANDLE;
    other.reflectModule_ = {}; // Zero-initialize the struct
    other.stage_ = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
}

Shader::~Shader()
{
    cleanup();
}

void Shader::cleanup()
{
    if (reflectModule_._internal != nullptr) {
        spvReflectDestroyShaderModule(&reflectModule_);
    }
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

SpvReflectShaderModule Shader::createRefModule(const vector<char>& shaderCode)
{
    SpvReflectShaderModule reflectShaderModule;
    SpvReflectResult reflectResult = spvReflectCreateShaderModule(
        shaderCode.size(), reinterpret_cast<const uint32_t*>(shaderCode.data()),
        &reflectShaderModule);
    if (reflectResult != SPV_REFLECT_RESULT_SUCCESS) {
        exitWithMessage("Failed to reflect shader module: {}",
                        getSpvReflectResultString(reflectResult));
    }
    if (reflectShaderModule._internal == nullptr) {
        exitWithMessage("Failed to create SPIR-V reflection module");
    }

    return reflectShaderModule;
}

vector<VkVertexInputAttributeDescription> Shader::makeVertexInputAttributeDescriptions() const
{
    vector<VkVertexInputAttributeDescription> attributes;

    // Only meaningful for vertex shaders
    if (stage_ != VK_SHADER_STAGE_VERTEX_BIT) {
        exitWithMessage("Only for vertex shaders.");
        return attributes;
    }

    // Enumerate input variables
    uint32_t varCount = reflectModule_.input_variable_count;
    if (varCount == 0 || reflectModule_.input_variables == nullptr) {
        printLog("[Warning] No input variables found in shader: {}", name_);
        return attributes;
    }

    attributes.reserve(varCount);

    vector<const SpvReflectInterfaceVariable*> inputVars(reflectModule_.input_variables,
                                                         reflectModule_.input_variables + varCount);
    sort(inputVars.begin(), inputVars.end(),
         [](const SpvReflectInterfaceVariable* a, const SpvReflectInterfaceVariable* b) {
             return a->location < b->location;
         }); // Sort by location

    // Example: match your struct layout
    // struct Vertex { float position[3]; float color[3]; };
    // print("{}, Stage: {}\n", name_, vkShaderStageFlagBitsToString(stage));
    uint32_t offset = 0;
    for (uint32_t i = 0; i < varCount; ++i) {
        const SpvReflectInterfaceVariable* var = inputVars[i];

        if (var->location == uint32_t(-1) || string(var->name) == "gl_VertexIndex") {
            // print("  No attribute\n"); // 셰이더에 in이 없는 경우
            continue;
        }

        assert(i == var->location);

        VkVertexInputAttributeDescription desc = {};
        desc.location = var->location;
        desc.binding = 0;
        desc.format = getVkFormatFromSpvReflectFormat(var->format);
        desc.offset = offset; // Offset은 나중에 계산할 예정

        attributes.push_back(desc);

        // print("  Attribute: name = {}, location = {}, binding = {}, format = {}, offset = {}\n",
        //       string(var->name), desc.location, desc.binding, vkFormatToString(desc.format),
        //       desc.offset);

        offset += getFormatSize(desc.format);
    }

    return attributes;
}

} // namespace hlab