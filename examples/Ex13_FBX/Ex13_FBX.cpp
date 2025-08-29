#define _CRT_SECURE_NO_WARNINGS

#include "engine/Logger.h"
#include "engine/Context.h"
#include "engine/StorageBuffer.h"
#include "engine/Image2D.h"
#include "engine/MappedBuffer.h"
#include "engine/Pipeline.h"
#include "engine/Window.h"
#include "engine/Swapchain.h"
#include "engine/Sampler.h"
#include "engine/GuiRenderer.h"
#include <format>
#include <fstream>
#include <filesystem> // For modern directory creation

// Platform-specific includes for directory creation
#ifdef _WIN32
#include <direct.h> // For _mkdir on Windows
#else
#include <sys/stat.h> // For mkdir on Unix/Linux
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// Assimp includes
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

using namespace hlab;

void saveTextureFromScene(const aiScene* scene, unsigned int textureIndex,
                          const std::string& outputDir)
{
    if (textureIndex >= scene->mNumTextures) {
        printLog("Warning: Texture index {} out of range", textureIndex);
        return;
    }

    const aiTexture* texture = scene->mTextures[textureIndex];
    std::string filename = outputDir + "/texture_" + std::to_string(textureIndex);

    if (texture->mHeight == 0) {
        // Compressed texture data
        std::string extension;
        if (texture->CheckFormat("png")) {
            extension = ".png";
        } else if (texture->CheckFormat("jpg") || texture->CheckFormat("jpeg")) {
            extension = ".jpg";
        } else if (texture->CheckFormat("dds")) {
            extension = ".dds";
        } else {
            extension = ".bin"; // Unknown format
        }

        filename += extension;

        // Write binary data directly
        std::ofstream file(filename, std::ios::binary);
        if (file.is_open()) {
            file.write(reinterpret_cast<const char*>(texture->pcData), texture->mWidth);
            file.close();
            printLog("Saved compressed texture: {}", filename);
        } else {
            printLog("Error: Could not write texture file: {}", filename);
        }
    } else {
        // Uncompressed RGBA data
        filename += ".png";

        // Convert RGBA to RGB if needed (STB expects specific formats)
        unsigned char* rgbData = new unsigned char[texture->mWidth * texture->mHeight * 3];
        const aiTexel* texelData = reinterpret_cast<const aiTexel*>(texture->pcData);

        for (unsigned int i = 0; i < texture->mWidth * texture->mHeight; ++i) {
            rgbData[i * 3 + 0] = texelData[i].r;
            rgbData[i * 3 + 1] = texelData[i].g;
            rgbData[i * 3 + 2] = texelData[i].b;
            // Note: ignoring alpha channel for PNG RGB
        }

        if (stbi_write_png(filename.c_str(), texture->mWidth, texture->mHeight, 3, rgbData,
                           texture->mWidth * 3)) {
            printLog("Saved uncompressed texture: {} ({}x{})", filename, texture->mWidth,
                     texture->mHeight);
        } else {
            printLog("Error: Could not write PNG file: {}", filename);
        }

        delete[] rgbData;
    }
}

// Capitalize first letter of texture type name
std::string capitalizeTextureName(const std::string& typeName)
{
    if (typeName.empty())
        return "";

    std::string result = typeName;
    result[0] = std::toupper(result[0]);
    return result;
}

void extractMaterialTextures(const aiScene* scene, const aiMaterial* material,
                             const std::string& outputDir)
{
    // Check different texture types
    aiTextureType textureTypes[] = {
        aiTextureType_DIFFUSE,           aiTextureType_SPECULAR,         aiTextureType_AMBIENT,
        aiTextureType_EMISSIVE,          aiTextureType_HEIGHT,           aiTextureType_NORMALS,
        aiTextureType_SHININESS,         aiTextureType_OPACITY,          aiTextureType_DISPLACEMENT,
        aiTextureType_LIGHTMAP,          aiTextureType_REFLECTION,       aiTextureType_BASE_COLOR,
        aiTextureType_NORMAL_CAMERA,     aiTextureType_EMISSION_COLOR,   aiTextureType_METALNESS,
        aiTextureType_DIFFUSE_ROUGHNESS, aiTextureType_AMBIENT_OCCLUSION};

    const char* textureTypeNames[] = {
        "diffuse",           "specular",         "ambient",       "emissive",       "height",
        "normals",           "shininess",        "opacity",       "displacement",   "lightmap",
        "reflection",        "base_color",       "normal_camera", "emission_color", "metalness",
        "diffuse_roughness", "ambient_occlusion"};

    for (int typeIndex = 0; typeIndex < sizeof(textureTypes) / sizeof(textureTypes[0]);
         ++typeIndex) {
        aiTextureType type = textureTypes[typeIndex];
        unsigned int textureCount = material->GetTextureCount(type);

        for (unsigned int texIndex = 0; texIndex < textureCount; ++texIndex) {
            aiString texturePath;
            if (material->GetTexture(type, texIndex, &texturePath) == AI_SUCCESS) {
                std::string pathStr = texturePath.C_Str();
                printLog("Found {} texture {}: {}", textureTypeNames[typeIndex], texIndex, pathStr);

                // Check if it's an embedded texture (starts with *)
                if (pathStr[0] == '*') {
                    // Embedded texture - extract index
                    int embeddedIndex = std::stoi(pathStr.substr(1));
                    if (embeddedIndex < static_cast<int>(scene->mNumTextures)) {
                        // Create clean texture name based on type
                        std::string cleanTypeName =
                            capitalizeTextureName(textureTypeNames[typeIndex]);
                        std::string outputFilename = outputDir + "/" + cleanTypeName;

                        if (textureCount > 1) {
                            outputFilename += "_" + std::to_string(texIndex);
                        }

                        const aiTexture* texture = scene->mTextures[embeddedIndex];

                        // Show the correspondence
                        printLog("-> Embedded texture index {} corresponds to {}", embeddedIndex,
                                 cleanTypeName);

                        if (texture->mHeight == 0) {
                            // Compressed texture
                            std::string extension = ".bin";
                            if (texture->CheckFormat("png"))
                                extension = ".png";
                            else if (texture->CheckFormat("jpg"))
                                extension = ".jpg";
                            else if (texture->CheckFormat("jpeg"))
                                extension = ".jpg";

                            outputFilename += extension;
                            std::ofstream file(outputFilename, std::ios::binary);
                            if (file.is_open()) {
                                file.write(reinterpret_cast<const char*>(texture->pcData),
                                           texture->mWidth);
                                file.close();
                                printLog("Saved embedded {} texture: {}",
                                         textureTypeNames[typeIndex], outputFilename);
                            }
                        } else {
                            // Uncompressed texture
                            outputFilename += ".png";
                            const aiTexel* texelData =
                                reinterpret_cast<const aiTexel*>(texture->pcData);

                            if (stbi_write_png(outputFilename.c_str(), texture->mWidth,
                                               texture->mHeight, 4, texelData,
                                               texture->mWidth * 4)) {
                                printLog("Saved embedded {} texture: {} ({}x{})",
                                         textureTypeNames[typeIndex], outputFilename,
                                         texture->mWidth, texture->mHeight);
                            }
                        }
                    }
                } else {
                    // External texture file - extract filename for comparison
                    std::filesystem::path extPath(pathStr);
                    std::string extFilename = extPath.filename().string();

                    printLog("External texture reference: {}", pathStr);
                    printLog("-> Filename: {}", extFilename);

                    // Try to copy external file if it exists
                    if (std::filesystem::exists(pathStr)) {
                        std::string cleanTypeName =
                            capitalizeTextureName(textureTypeNames[typeIndex]);
                        std::string outputFilename = outputDir + "/" + cleanTypeName + ".png";

                        try {
                            std::filesystem::copy_file(
                                pathStr, outputFilename,
                                std::filesystem::copy_options::overwrite_existing);
                            printLog("Copied external texture to: {}", outputFilename);
                        } catch (const std::filesystem::filesystem_error& ex) {
                            printLog("Failed to copy external texture: {}", ex.what());
                        }
                    }
                }
            }
        }
    }
}

void processFbxModel(const std::string& fbxFilename)
{
    Assimp::Importer importer;

    // Set post-processing flags
    unsigned int postProcessFlags =
        aiProcess_Triangulate |           // Convert polygons to triangles
        aiProcess_FlipUVs |               // Flip UV coordinates (for OpenGL/Vulkan)
        aiProcess_CalcTangentSpace |      // Calculate tangent vectors
        aiProcess_GenSmoothNormals |      // Generate smooth normals if missing
        aiProcess_JoinIdenticalVertices | // Remove duplicate vertices
        aiProcess_SortByPType |           // Sort primitives by type
        aiProcess_ValidateDataStructure | // Validate the data structure
        aiProcess_ImproveCacheLocality;   // Optimize vertex cache locality

    // Load the FBX file
    const aiScene* scene = importer.ReadFile(fbxFilename, postProcessFlags);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        printLog("Error loading FBX file: {}", importer.GetErrorString());
        return;
    }

    printLog("Successfully loaded FBX file: {}", fbxFilename);
    printLog("  Meshes: {}", scene->mNumMeshes);
    printLog("  Materials: {}", scene->mNumMaterials);
    printLog("  Textures: {}", scene->mNumTextures);
    printLog("  Animations: {}", scene->mNumAnimations);

    // Get the directory where the FBX file is located
    std::filesystem::path fbxPath(fbxFilename);
    std::string outputDir = fbxPath.parent_path().string();

    printLog("Output directory: {}", outputDir);

    // Extract embedded textures
    printLog("\nExtracting embedded textures...");
    for (unsigned int i = 0; i < scene->mNumTextures; ++i) {
        saveTextureFromScene(scene, i, outputDir);
    }

    // Process materials and their textures
    printLog("\nProcessing materials...");
    for (unsigned int i = 0; i < scene->mNumMaterials; ++i) {
        const aiMaterial* material = scene->mMaterials[i];

        aiString materialName;
        if (material->Get(AI_MATKEY_NAME, materialName) == AI_SUCCESS) {
            printLog("Material {}: {}", i, materialName.C_Str());
        } else {
            printLog("Material {}: (unnamed)", i);
        }

        // Extract textures for this material
        extractMaterialTextures(scene, material, outputDir);
    }

    // Print mesh information
    printLog("\nMesh information:");
    for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
        const aiMesh* mesh = scene->mMeshes[i];
        printLog("  Mesh {}: {} vertices, {} faces, material index: {}", i, mesh->mNumVertices,
                 mesh->mNumFaces, mesh->mMaterialIndex);

        // Check what vertex attributes are available
        std::string attributes = "Attributes: ";
        if (mesh->HasPositions())
            attributes += "positions ";
        if (mesh->HasNormals())
            attributes += "normals ";
        if (mesh->HasTangentsAndBitangents())
            attributes += "tangents ";
        if (mesh->HasTextureCoords(0))
            attributes += "UV0 ";
        if (mesh->HasTextureCoords(1))
            attributes += "UV1 ";
        if (mesh->HasVertexColors(0))
            attributes += "colors ";

        printLog("    {}", attributes);
    }

    printLog("\nTexture extraction completed! Check the same directory as the FBX file.");
}

int main()
{
    const string fbxFilename = "../../assets/characters/Leonard/Leonard.fbx";

    printLog("=== FBX Model and Texture Extractor ===");
    printLog("Processing file: {}", fbxFilename);

    // Check if file exists
    std::ifstream file(fbxFilename);
    if (!file.good()) {
        printLog("Error: FBX file not found: {}", fbxFilename);
        printLog("Please ensure the file exists at the specified path.");
        return -1;
    }
    file.close();

    // Process the FBX model and extract textures
    processFbxModel(fbxFilename);

    printLog("Process completed!");
    return 0;
}
