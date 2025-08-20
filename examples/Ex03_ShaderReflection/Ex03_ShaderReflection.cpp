#include <spirv-reflect/spirv_reflect.h>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>

using namespace std;

vector<char> readSpvFile(const string& spvFilename)
{
    // Validate file extension - SPIR-V files must have .spv extension
    if (spvFilename.length() < 4 || spvFilename.substr(spvFilename.length() - 4) != ".spv") {
        cerr << "Shader file does not have .spv extension: " << spvFilename << endl;
        exit(1);
    }

    // Open file in binary mode, positioned at end for size calculation
    ifstream is(spvFilename, ios::binary | ios::in | ios::ate);
    if (!is.is_open()) {
        cerr << "Could not open shader file: " << spvFilename << endl;
        exit(1);
    }

    // Get file size and validate it's a valid SPIR-V file
    size_t shaderSize = static_cast<size_t>(is.tellg());
    if (shaderSize == 0 || shaderSize % 4 != 0) {
        cerr << "Shader file size is invalid (must be >0 and multiple of 4): " << spvFilename
             << endl;
        exit(1);
    }

    // Reset to beginning and read entire file
    is.seekg(0, ios::beg);
    vector<char> shaderCode(shaderSize);
    is.read(shaderCode.data(), shaderSize);
    is.close();

    return shaderCode;
}

const char* getShaderStageString(SpvReflectShaderStageFlagBits stage)
{
    switch (stage) {
    case SPV_REFLECT_SHADER_STAGE_VERTEX_BIT:
        return "Vertex";
    case SPV_REFLECT_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
        return "Tessellation Control";
    case SPV_REFLECT_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
        return "Tessellation Evaluation";
    case SPV_REFLECT_SHADER_STAGE_GEOMETRY_BIT:
        return "Geometry";
    case SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT:
        return "Fragment";
    case SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT:
        return "Compute";
    default:
        return "Unknown";
    }
}

const char* getDescriptorTypeString(SpvReflectDescriptorType type)
{
    switch (type) {
    case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
        return "Sampler";
    case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        return "Combined Image Sampler";
    case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        return "Sampled Image";
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        return "Storage Image";
    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        return "Uniform Texel Buffer";
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        return "Storage Texel Buffer";
    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        return "Uniform Buffer";
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        return "Storage Buffer";
    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        return "Dynamic Uniform Buffer";
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        return "Dynamic Storage Buffer";
    case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        return "Input Attachment";
    default:
        return "Unknown";
    }
}

void printReflectionInfo(const SpvReflectShaderModule& reflectModule)
{
    cout << "=== SPIR-V Shader Reflection Information ===" << endl;
    cout << "Entry Point: "
         << (reflectModule.entry_point_name ? reflectModule.entry_point_name : "Unknown") << endl;
    cout << "Shader Stage: " << getShaderStageString(reflectModule.shader_stage) << endl;
    cout << "Source Language: ";
    switch (reflectModule.source_language) {
    case SpvSourceLanguageGLSL:
        cout << "GLSL";
        break;
    case SpvSourceLanguageHLSL:
        cout << "HLSL";
        break;
    case SpvSourceLanguageOpenCL_C:
        cout << "OpenCL C";
        break;
    default:
        cout << "Unknown (" << reflectModule.source_language << ")";
        break;
    }
    cout << " v" << reflectModule.source_language_version << endl;

    if (reflectModule.source_file) {
        cout << "Source File: " << reflectModule.source_file << endl;
    }

    cout << "\n--- Descriptor Bindings ---" << endl;
    cout << "Total descriptor bindings: " << reflectModule.descriptor_binding_count << endl;

    for (uint32_t i = 0; i < reflectModule.descriptor_binding_count; ++i) {
        const SpvReflectDescriptorBinding* binding = &reflectModule.descriptor_bindings[i];
        cout << "  Binding " << i << ":" << endl;
        cout << "    Name: " << (binding->name ? binding->name : "Unknown") << endl;
        cout << "    Set: " << binding->set << endl;
        cout << "    Binding: " << binding->binding << endl;
        cout << "    Type: " << getDescriptorTypeString(binding->descriptor_type) << endl;
        cout << "    Count: " << binding->count << endl;

        if (binding->image.dim != SpvDimMax) {
            cout << "    Image Dimension: ";
            switch (binding->image.dim) {
            case SpvDim1D:
                cout << "1D";
                break;
            case SpvDim2D:
                cout << "2D";
                break;
            case SpvDim3D:
                cout << "3D";
                break;
            case SpvDimCube:
                cout << "Cube";
                break;
            case SpvDimBuffer:
                cout << "Buffer";
                break;
            default:
                cout << "Unknown";
                break;
            }
            cout << endl;
            cout << "    Image Format: " << binding->image.image_format << endl;
        }
    }

    cout << "\n--- Descriptor Sets ---" << endl;
    cout << "Total descriptor sets: " << reflectModule.descriptor_set_count << endl;
    for (uint32_t i = 0; i < reflectModule.descriptor_set_count; ++i) {
        const SpvReflectDescriptorSet* set = &reflectModule.descriptor_sets[i];
        cout << "  Set " << set->set << ": " << set->binding_count << " bindings" << endl;
    }

    cout << "\n--- Input Variables ---" << endl;
    cout << "Total input variables: " << reflectModule.input_variable_count << endl;
    for (uint32_t i = 0; i < reflectModule.input_variable_count; ++i) {
        const SpvReflectInterfaceVariable* var = reflectModule.input_variables[i];
        cout << "  Input " << i << ": " << (var->name ? var->name : "Unknown") << endl;
        cout << "    Location: " << var->location << endl;
    }

    cout << "\n--- Output Variables ---" << endl;
    cout << "Total output variables: " << reflectModule.output_variable_count << endl;
    for (uint32_t i = 0; i < reflectModule.output_variable_count; ++i) {
        const SpvReflectInterfaceVariable* var = reflectModule.output_variables[i];
        cout << "  Output " << i << ": " << (var->name ? var->name : "Unknown") << endl;
        cout << "    Location: " << var->location << endl;
    }

    cout << "\n--- Push Constants ---" << endl;
    cout << "Total push constant blocks: " << reflectModule.push_constant_block_count << endl;
    for (uint32_t i = 0; i < reflectModule.push_constant_block_count; ++i) {
        const SpvReflectBlockVariable* block = &reflectModule.push_constant_blocks[i];
        cout << "  Push constant block " << i << ":" << endl;
        cout << "    Name: " << (block->name ? block->name : "Unknown") << endl;
        cout << "    Size: " << block->size << " bytes" << endl;
        cout << "    Offset: " << block->offset << endl;
    }

    // For compute shaders, show workgroup size
    if (reflectModule.shader_stage & SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT) {
        cout << "\n--- Compute Shader Info ---" << endl;
        cout << "Local workgroup size: (" << reflectModule.entry_points[0].local_size.x << ", "
             << reflectModule.entry_points[0].local_size.y << ", "
             << reflectModule.entry_points[0].local_size.z << ")" << endl;
    }
}

int main()
{
    string assetsPath = "../../assets/";
    string computeShaderFilename = assetsPath + "shaders/test.comp.spv";

    SpvReflectShaderModule reflectModule_{};

    // Read the SPIR-V file
    cout << "Reading SPIR-V file: " << computeShaderFilename << endl;
    vector<char> shaderCode = readSpvFile(computeShaderFilename);
    cout << "File size: " << shaderCode.size() << " bytes" << endl;

    // Initialize the reflection module
    SpvReflectResult result = spvReflectCreateShaderModule(
        shaderCode.size(), reinterpret_cast<const uint32_t*>(shaderCode.data()), &reflectModule_);

    if (result != SPV_REFLECT_RESULT_SUCCESS) {
        cerr << "Failed to create SPIR-V reflection module. Error code: " << result << endl;
        return 1;
    }

    // Print reflection information
    printReflectionInfo(reflectModule_);

    // Clean up the reflection module
    spvReflectDestroyShaderModule(&reflectModule_);

    cout << "\nShader reflection completed successfully!" << endl;
    return 0;
}