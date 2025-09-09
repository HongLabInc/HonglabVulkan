#include "ModelLoader.h"
#include "Model.h"
#include <chrono>
#include <filesystem>
#include <stb_image.h>
#include <stb_image_write.h>
#include <glm/gtc/type_ptr.hpp> // For glm::make_mat4

namespace hlab {

ModelLoader::ModelLoader(Model& model) : model_(model)
{
}

void ModelLoader::loadFromModelFile(const string& modelFilename, bool readBistroObj)
{
    // Start timer for loading time measurement
    auto startTime = std::chrono::high_resolution_clock::now();

    // Generate cache file path based on model filename
    filesystem::path modelPath(modelFilename);
    string cacheFilename = modelPath.stem().string() + "_cache.bin";
    filesystem::path cachePath = modelPath.parent_path() / cacheFilename;

    // Check if cache file exists and is newer than the model file
    bool useCache = false;
    if (readBistroObj && filesystem::exists(cachePath)) {
        useCache = true;
    }

    // useCache = false; // 디버깅용 캐시 사용 중단

    // Try to load from cache first
    if (useCache) {
        loadFromCache(cachePath.string());

        // Check if cache loading was successful (non-empty model)
        if (!model_.meshes_.empty() && !model_.materials_.empty()) {
            // Load textures after successful cache load
            model_.textures_.reserve(model_.textureFilenames_.size());
            for (auto& filename : model_.textureFilenames_) {
                string prefix = readBistroObj ? directory_ + "/LowRes/" : "";
                model_.textures_.emplace_back(make_unique<Image2D>(model_.ctx_));
                model_.textures_.back()->createTextureFromImage(
                    prefix + filename, false, model_.textureSRgb_[model_.textures_.size() - 1]);
            }

            // Calculate elapsed time
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

            printLog("Successfully loaded model from cache: {}", cachePath.string());
            printLog("  Meshes: {}", model_.meshes_.size());
            printLog("  Materials: {}", model_.materials_.size());
            printLog("  Loading time: {} ms", duration.count());
            return;
        } else {
            // Cache loading failed, clear any partially loaded data and fall back to model loading
            printLog("Cache loading failed, falling back to model file loading");
            model_.cleanup();
        }
    }

    // Load from model file (original code)
    uint32_t importFlags = aiProcess_Triangulate;

    if (readBistroObj) {
        importFlags = 0 | aiProcess_JoinIdenticalVertices | aiProcess_Triangulate |
                      aiProcess_GenSmoothNormals | aiProcess_LimitBoneWeights |
                      aiProcess_SplitLargeMeshes | aiProcess_ImproveCacheLocality |
                      aiProcess_RemoveRedundantMaterials | aiProcess_FindDegenerates |
                      aiProcess_FindInvalidData | aiProcess_GenUVCoords;
    }

    const aiScene* scene = importer_.ReadFile(modelFilename, importFlags);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        exitWithMessage("ERROR::ASSIMP: {}", importer_.GetErrorString());
        return;
    }

    // Improved directory extraction using filesystem::path for cross-platform compatibility
    filesystem::path modelPath2(modelFilename);
    directory_ = modelPath2.parent_path().string();
    if (directory_.empty()) {
        directory_ = "."; // Current directory if no path specified
    }

    printLog("Model directory: {}", directory_);

    // Store global inverse transform for the Model class
    mat4 globalInverseTransform =
        glm::inverse(glm::make_mat4(&scene->mRootNode->mTransformation.a1));
    model_.globalInverseTransform_ = globalInverseTransform;

    // Process materials first
    model_.materials_.resize(scene->mNumMaterials);
    for (uint32_t i = 0; i < scene->mNumMaterials; i++) {
        if (readBistroObj) {
            processMaterialBistro(scene->mMaterials[i], scene, i);
        } else {
            processMaterial(scene->mMaterials[i], scene, i);
        }
    }

    // IMPORTANT: Process animations and bones BEFORE processing nodes/meshes
    // This ensures the Animation system has the global bone mapping ready
    // when processMesh needs to assign global bone indices to vertices
    printLog("Processing animations and bones before mesh processing...");
    processAnimations(scene);
    processBones(scene);

    // AFTER animation processing, synchronize the global inverse transform
    if (model_.animation_) {
        model_.animation_->setGlobalInverseTransform(globalInverseTransform);
        printLog("Synchronized global inverse transform between Model and Animation systems");
    }

    // Now process nodes and meshes - they can use the global bone indices
    processNode(scene->mRootNode, scene);
    model_.calculateBoundingBox();

    // 안내: Bistro 모델은 파이썬 스크립트로 전처리한 저해상도 텍스쳐를 읽어들입니다.
    model_.textures_.reserve(model_.textureFilenames_.size());
    for (auto& filename : model_.textureFilenames_) {
        string prefix = readBistroObj ? directory_ + "/LowRes/" : directory_ + "/";
        model_.textures_.emplace_back(make_unique<Image2D>(model_.ctx_));
        // Check if this is an embedded texture (indicated by * prefix)
        if (!filename.empty() && filename[0] == '*') {
            // Parse the texture index from the path (e.g., "*0" -> 0)
            int textureIndex = stoi(filename.substr(1));

            // Get the embedded texture from the scene
            const aiScene* scene = importer_.GetScene();
            if (scene && textureIndex < static_cast<int>(scene->mNumTextures)) {
                const aiTexture* aiTex = scene->mTextures[textureIndex];

                int width, height, channels;
                unsigned char* data = nullptr;

                if (aiTex->mHeight == 0) {
                    // Compressed texture data (e.g., PNG, JPG)
                    data = stbi_load_from_memory(
                        reinterpret_cast<const unsigned char*>(aiTex->pcData), aiTex->mWidth,
                        &width, &height, &channels, STBI_rgb_alpha);
                } else {
                    // Uncompressed RGBA texture data
                    width = aiTex->mWidth;
                    height = aiTex->mHeight;
                    channels = 4;

                    // Convert aiTexel to RGBA8
                    size_t dataSize = width * height * 4;
                    data = static_cast<unsigned char*>(malloc(dataSize));

                    for (int i = 0; i < width * height; ++i) {
                        data[i * 4 + 0] = static_cast<unsigned char>(aiTex->pcData[i].r * 255);
                        data[i * 4 + 1] = static_cast<unsigned char>(aiTex->pcData[i].g * 255);
                        data[i * 4 + 2] = static_cast<unsigned char>(aiTex->pcData[i].b * 255);
                        data[i * 4 + 3] = static_cast<unsigned char>(aiTex->pcData[i].a * 255);
                    }
                }

                if (data) {
                    // Create texture directly from memory data
                    model_.textures_.back()->createFromPixelData(
                        data, width, height, 4, model_.textureSRgb_[model_.textures_.size() - 1]);

                    // Free memory
                    if (aiTex->mHeight == 0) {
                        stbi_image_free(data); // Free stbi allocated memory
                    } else {
                        free(data); // Free manually allocated memory
                    }

                    printLog("Loaded embedded texture {} ({}x{}) with {} format", textureIndex,
                             width, height,
                             model_.textureSRgb_[model_.textures_.size() - 1] ? "sRGB" : "linear");
                } else {
                    printLog("WARNING: Failed to decode embedded texture {}", textureIndex);
                    if (aiTex->mHeight == 0) {
                        printLog("  Reason: {}", stbi_failure_reason());
                    }
                }
            } else {
                printLog("WARNING: Embedded texture index {} out of range (max: {})", textureIndex,
                         scene ? scene->mNumTextures : 0);
            }
        } else {
            // External texture file - use existing path logic

            string prefix = readBistroObj ? directory_ + "/LowRes/" : directory_ + "/";
            // textures_.back().createTextureFromImage(prefix + filename, false,
            //                                         textureSRgb_[textures_.size() - 1]);

            // 안내:
            // - 캐릭터 fbx는 미리 추출한 텍스쳐 사용
            // - 같은 폴더에 있기 때문에 파일이름에서 폴더명 제거
            string shortFilename =
                readBistroObj ? filename : filesystem::path(filename).filename().string();

            printLog("Texture filename: {}", prefix + shortFilename);

            model_.textures_.back()->createTextureFromImage(
                prefix + shortFilename, false, model_.textureSRgb_[model_.textures_.size() - 1]);
        }
    }

    // Calculate elapsed time
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    printLog("Successfully loaded model: {}", modelFilename);
    printLog("  Meshes: {}", model_.meshes_.size());
    printLog("  Materials: {}", model_.materials_.size());
    printLog("  Loading time: {} ms", duration.count());

    if (readBistroObj && !useCache) {
        optimizeMeshesBistro();
        writeToCache(cachePath.string());
        printLog("Model cached to: {}", cachePath.string());
    }

    return;
}

void ModelLoader::loadFromCache(const string& cacheFilename)
{
    std::ifstream stream(cacheFilename, std::ios::binary);
    if (!stream.is_open()) {
        // Cache file doesn't exist or cannot be opened
        return;
    }

    try {
        // Read file format version for future compatibility
        uint32_t fileVersion;
        stream.read(reinterpret_cast<char*>(&fileVersion), sizeof(fileVersion));
        if (!stream.good() || fileVersion != 1) {
            return; // Unsupported version or read error
        }

        // Read directory
        uint32_t dirLength;
        stream.read(reinterpret_cast<char*>(&dirLength), sizeof(dirLength));
        if (!stream.good())
            return;

        if (dirLength > 0) {
            directory_.resize(dirLength);
            stream.read(&directory_[0], dirLength);
            if (!stream.good())
                return;
        }

        // Read global inverse transform
        stream.read(reinterpret_cast<char*>(&model_.globalInverseTransform_),
                    sizeof(model_.globalInverseTransform_));
        if (!stream.good())
            return;

        // Read bounding box
        stream.read(reinterpret_cast<char*>(&model_.boundingBoxMin_),
                    sizeof(model_.boundingBoxMin_));
        stream.read(reinterpret_cast<char*>(&model_.boundingBoxMax_),
                    sizeof(model_.boundingBoxMax_));
        if (!stream.good())
            return;

        // Read texture filenames
        uint32_t textureCount;
        stream.read(reinterpret_cast<char*>(&textureCount), sizeof(textureCount));
        if (!stream.good())
            return;

        model_.textureFilenames_.clear();
        model_.textureSRgb_.clear();
        model_.textureFilenames_.reserve(textureCount);
        model_.textureSRgb_.reserve(textureCount);

        for (uint32_t i = 0; i < textureCount; ++i) {
            uint32_t filenameLength;
            stream.read(reinterpret_cast<char*>(&filenameLength), sizeof(filenameLength));
            if (!stream.good())
                return;

            string filename;
            if (filenameLength > 0) {
                filename.resize(filenameLength);
                stream.read(&filename[0], filenameLength);
                if (!stream.good())
                    return;
            }
            model_.textureFilenames_.push_back(std::move(filename));

            bool sRGB;
            stream.read(reinterpret_cast<char*>(&sRGB), sizeof(sRGB));
            if (!stream.good())
                return;
            model_.textureSRgb_.push_back(sRGB);
        }

        // Read meshes
        uint32_t meshCount;
        stream.read(reinterpret_cast<char*>(&meshCount), sizeof(meshCount));
        if (!stream.good())
            return;

        model_.meshes_.clear();
        model_.meshes_.resize(meshCount);
        for (uint32_t i = 0; i < meshCount; ++i) {
            if (!model_.meshes_[i].readFromBinaryFileStream(stream)) {
                return;
            }
        }

        // Read materials
        uint32_t materialCount;
        stream.read(reinterpret_cast<char*>(&materialCount), sizeof(materialCount));
        if (!stream.good())
            return;

        model_.materials_.clear();
        model_.materials_.resize(materialCount);
        for (uint32_t i = 0; i < materialCount; ++i) {
            model_.materials_[i].loadFromCache(
                ""); // Use empty string since we're reading from stream

            // Read material data directly from our stream
            uint32_t materialVersion;
            stream.read(reinterpret_cast<char*>(&materialVersion), sizeof(materialVersion));
            if (!stream.good() || materialVersion != 1)
                return;

            // Read material name
            uint32_t nameLength;
            stream.read(reinterpret_cast<char*>(&nameLength), sizeof(nameLength));
            if (!stream.good())
                return;

            if (nameLength > 0) {
                model_.materials_[i].name_.resize(nameLength);
                stream.read(&model_.materials_[i].name_[0], nameLength);
                if (!stream.good())
                    return;
            }

            // Read material properties
            stream.read(reinterpret_cast<char*>(&model_.materials_[i].ubo_.emissiveFactor_),
                        sizeof(model_.materials_[i].ubo_.emissiveFactor_));
            stream.read(reinterpret_cast<char*>(&model_.materials_[i].ubo_.baseColorFactor_),
                        sizeof(model_.materials_[i].ubo_.baseColorFactor_));
            stream.read(reinterpret_cast<char*>(&model_.materials_[i].ubo_.roughness_),
                        sizeof(model_.materials_[i].ubo_.roughness_));
            stream.read(reinterpret_cast<char*>(&model_.materials_[i].ubo_.transparencyFactor_),
                        sizeof(model_.materials_[i].ubo_.transparencyFactor_));
            stream.read(reinterpret_cast<char*>(&model_.materials_[i].ubo_.discardAlpha_),
                        sizeof(model_.materials_[i].ubo_.discardAlpha_));
            stream.read(reinterpret_cast<char*>(&model_.materials_[i].ubo_.metallicFactor_),
                        sizeof(model_.materials_[i].ubo_.metallicFactor_));

            // Read texture indices
            stream.read(reinterpret_cast<char*>(&model_.materials_[i].ubo_.baseColorTextureIndex_),
                        sizeof(model_.materials_[i].ubo_.baseColorTextureIndex_));
            stream.read(reinterpret_cast<char*>(&model_.materials_[i].ubo_.emissiveTextureIndex_),
                        sizeof(model_.materials_[i].ubo_.emissiveTextureIndex_));
            stream.read(reinterpret_cast<char*>(&model_.materials_[i].ubo_.normalTextureIndex_),
                        sizeof(model_.materials_[i].ubo_.normalTextureIndex_));
            stream.read(reinterpret_cast<char*>(&model_.materials_[i].ubo_.opacityTextureIndex_),
                        sizeof(model_.materials_[i].ubo_.opacityTextureIndex_));
            stream.read(
                reinterpret_cast<char*>(&model_.materials_[i].ubo_.metallicRoughnessTextureIndex_),
                sizeof(model_.materials_[i].ubo_.metallicRoughnessTextureIndex_));
            stream.read(reinterpret_cast<char*>(&model_.materials_[i].ubo_.occlusionTextureIndex_),
                        sizeof(model_.materials_[i].ubo_.occlusionTextureIndex_));

            // Read flags
            stream.read(reinterpret_cast<char*>(&model_.materials_[i].flags_),
                        sizeof(model_.materials_[i].flags_));
            if (!stream.good())
                return;
        }

        // Initialize an empty root node - the hierarchical structure
        // is complex and might not be worth caching for performance gains
        model_.rootNode_ = make_unique<ModelNode>();
        model_.rootNode_->name = "Root";

        // Textures will need to be reloaded from files since they contain
        // device-specific Vulkan resources that can't be serialized
        model_.textures_.clear();

    } catch (...) {
        // If any error occurs, clear data and continue with empty model
        model_.meshes_.clear();
        model_.materials_.clear();
        model_.textureFilenames_.clear();
        model_.textureSRgb_.clear();
        model_.textures_.clear();
    }
}

void ModelLoader::writeToCache(const string& cacheFilename)
{
    std::ofstream stream(cacheFilename, std::ios::binary);
    if (!stream.is_open()) {
        return; // Cannot create cache file
    }

    try {
        // Write file format version for future compatibility
        const uint32_t fileVersion = 1;
        stream.write(reinterpret_cast<const char*>(&fileVersion), sizeof(fileVersion));

        // Write directory
        uint32_t dirLength = static_cast<uint32_t>(directory_.length());
        stream.write(reinterpret_cast<const char*>(&dirLength), sizeof(dirLength));
        if (dirLength > 0) {
            stream.write(directory_.c_str(), dirLength);
        }

        // Write global inverse transform
        stream.write(reinterpret_cast<const char*>(&model_.globalInverseTransform_),
                     sizeof(model_.globalInverseTransform_));

        // Write bounding box
        stream.write(reinterpret_cast<const char*>(&model_.boundingBoxMin_),
                     sizeof(model_.boundingBoxMin_));
        stream.write(reinterpret_cast<const char*>(&model_.boundingBoxMax_),
                     sizeof(model_.boundingBoxMax_));

        // Write texture filenames and sRGB flags
        uint32_t textureCount = static_cast<uint32_t>(model_.textureFilenames_.size());
        stream.write(reinterpret_cast<const char*>(&textureCount), sizeof(textureCount));

        for (uint32_t i = 0; i < textureCount; ++i) {
            uint32_t filenameLength = static_cast<uint32_t>(model_.textureFilenames_[i].length());
            stream.write(reinterpret_cast<const char*>(&filenameLength), sizeof(filenameLength));
            if (filenameLength > 0) {
                stream.write(model_.textureFilenames_[i].c_str(), filenameLength);
            }

            bool sRGB = (i < model_.textureSRgb_.size()) ? model_.textureSRgb_[i] : false;
            stream.write(reinterpret_cast<const char*>(&sRGB), sizeof(sRGB));
        }

        // Write meshes
        uint32_t meshCount = static_cast<uint32_t>(model_.meshes_.size());
        stream.write(reinterpret_cast<const char*>(&meshCount), sizeof(meshCount));

        for (const auto& mesh : model_.meshes_) {
            if (!mesh.writeToBinaryFileStream(stream)) {
                return;
            }
        }

        // Write materials
        uint32_t materialCount = static_cast<uint32_t>(model_.materials_.size());
        stream.write(reinterpret_cast<const char*>(&materialCount), sizeof(materialCount));

        for (const auto& material : model_.materials_) {
            // Write material data directly to our stream (similar to Material::writeToCache)
            const uint32_t materialVersion = 1;
            stream.write(reinterpret_cast<const char*>(&materialVersion), sizeof(materialVersion));

            // Write material name
            uint32_t nameLength = static_cast<uint32_t>(material.name_.length());
            stream.write(reinterpret_cast<const char*>(&nameLength), sizeof(nameLength));
            if (nameLength > 0) {
                stream.write(material.name_.c_str(), nameLength);
            }

            // Write material properties
            stream.write(reinterpret_cast<const char*>(&material.ubo_.emissiveFactor_),
                         sizeof(material.ubo_.emissiveFactor_));
            stream.write(reinterpret_cast<const char*>(&material.ubo_.baseColorFactor_),
                         sizeof(material.ubo_.baseColorFactor_));
            stream.write(reinterpret_cast<const char*>(&material.ubo_.roughness_),
                         sizeof(material.ubo_.roughness_));
            stream.write(reinterpret_cast<const char*>(&material.ubo_.transparencyFactor_),
                         sizeof(material.ubo_.transparencyFactor_));
            stream.write(reinterpret_cast<const char*>(&material.ubo_.discardAlpha_),
                         sizeof(material.ubo_.discardAlpha_));
            stream.write(reinterpret_cast<const char*>(&material.ubo_.metallicFactor_),
                         sizeof(material.ubo_.metallicFactor_));

            // Write texture indices
            stream.write(reinterpret_cast<const char*>(&material.ubo_.baseColorTextureIndex_),
                         sizeof(material.ubo_.baseColorTextureIndex_));
            stream.write(reinterpret_cast<const char*>(&material.ubo_.emissiveTextureIndex_),
                         sizeof(material.ubo_.emissiveTextureIndex_));
            stream.write(reinterpret_cast<const char*>(&material.ubo_.normalTextureIndex_),
                         sizeof(material.ubo_.normalTextureIndex_));
            stream.write(reinterpret_cast<const char*>(&material.ubo_.opacityTextureIndex_),
                         sizeof(material.ubo_.opacityTextureIndex_));
            stream.write(
                reinterpret_cast<const char*>(&material.ubo_.metallicRoughnessTextureIndex_),
                sizeof(material.ubo_.metallicRoughnessTextureIndex_));
            stream.write(reinterpret_cast<const char*>(&material.ubo_.occlusionTextureIndex_),
                         sizeof(material.ubo_.occlusionTextureIndex_));

            // Write flags
            stream.write(reinterpret_cast<const char*>(&material.flags_), sizeof(material.flags_));
        }

    } catch (...) {
        // If any error occurs, silently ignore
    }
}

void ModelLoader::processNode(aiNode* node, const aiScene* scene, ModelNode* parent)
{
    // Create new ModelNode
    unique_ptr<ModelNode> modelNode = make_unique<ModelNode>();
    modelNode->name = string(node->mName.C_Str());
    modelNode->parent = parent;

    modelNode->localMatrix = glm::transpose(glm::make_mat4(&node->mTransformation.a1));

    // Extract transformation components if needed
    aiVector3D scaling, position;
    aiQuaternion rotation;
    node->mTransformation.Decompose(scaling, rotation, position);

    modelNode->translation = vec3(position.x, position.y, position.z);
    modelNode->rotation = quat(rotation.w, rotation.x, rotation.y, rotation.z);
    modelNode->scale = vec3(scaling.x, scaling.y, scaling.z);

    // Process all meshes in this node
    for (uint32_t i = 0; i < node->mNumMeshes; i++) {
        uint32_t meshIndex = node->mMeshes[i];

        // Ensure meshes_ vector is large enough
        if (meshIndex >= model_.meshes_.size()) {
            model_.meshes_.resize(meshIndex + 1);
        }

        // Process the mesh if it hasn't been processed yet
        if (model_.meshes_[meshIndex].vertices_.empty()) {
            processMesh(scene->mMeshes[meshIndex], scene, meshIndex);
        }

        modelNode->meshIndices.push_back(meshIndex);
    }

    // Store pointer to current node for child processing
    ModelNode* currentNode = modelNode.get();

    // Add to parent or root
    if (parent) {
        parent->children.push_back(move(modelNode));
    } else {
        model_.rootNode_ = move(modelNode);
    }

    // Process children
    for (uint32_t i = 0; i < node->mNumChildren; i++) {
        processNode(node->mChildren[i], scene, currentNode);
    }
}

void ModelLoader::processMesh(aiMesh* mesh, const aiScene* scene, uint32_t meshIndex)
{
    auto& meshes = model_.meshes_;

    // Ensure meshes_ vector is large enough
    if (meshIndex >= meshes.size()) {
        meshes.resize(meshIndex + 1);
    }

    Mesh& currentMesh = meshes[meshIndex];

    currentMesh.name_ = string(mesh->mName.C_Str());

    // Process vertices with UV validation
    currentMesh.vertices_.reserve(mesh->mNumVertices);
    for (uint32_t i = 0; i < mesh->mNumVertices; i++) {
        Vertex vertex;

        vertex.position = vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);

        // Normal
        if (mesh->HasNormals()) {
            vertex.normal = vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
        } else {
            vertex.normal = vec3(0.0f, 1.0f, 0.0f);
        }

        // Texture coordinates with validation
        if (mesh->mTextureCoords[0]) {
            vertex.texCoord = vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
            vertex.texCoord.y = 1.0f - vertex.texCoord.y; // y fliped
        } else {
            vertex.texCoord = vec2(0.0f, 0.0f);
            currentMesh.noTextureCoords = true;
        }

        // Tangent
        if (mesh->HasTangentsAndBitangents()) {
            vertex.tangent = vec3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
            vertex.bitangent =
                vec3(mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z);
        } else {
            vertex.tangent = vec3(1.0f, 0.0f, 0.0f);
            vertex.bitangent = vec3(0.0f, 0.0f, 1.0f);
        }

        currentMesh.vertices_.push_back(vertex);
    }

    // Process indices
    currentMesh.indices_.reserve(mesh->mNumFaces * 3);
    for (uint32_t i = 0; i < mesh->mNumFaces; i++) {
        if (mesh->mFaces[i].mNumIndices != 3)
            continue;
        aiFace face = mesh->mFaces[i];
        for (uint32_t j = 0; j < face.mNumIndices; j++) {
            currentMesh.indices_.push_back(face.mIndices[j]);
        }
    }

    // Set material index
    currentMesh.materialIndex_ = mesh->mMaterialIndex;

    // Calculate bounds immediately after vertices are populated
    currentMesh.calculateBounds();

    // Process bone weights and indices for skeletal animation
    if (mesh->HasBones()) {
        printLog("Processing {} bones for mesh '{}'", mesh->mNumBones, mesh->mName.C_Str());

        // First pass: collect bone weights for each vertex using GLOBAL bone indices
        for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
            const aiBone* bone = mesh->mBones[boneIndex];
            string boneName = bone->mName.C_Str();

            // Get the GLOBAL bone index from the Animation system
            int globalBoneIndex = -1;
            if (model_.animation_) {
                globalBoneIndex =
                    model_.animation_->getGlobalBoneIndex(boneName); // FIXED: Call as method
            }

            if (globalBoneIndex == -1) {
                printLog(
                    "WARNING: Bone '{}' not found in global bone mapping, using local index {}",
                    boneName, boneIndex);
                globalBoneIndex = static_cast<int>(boneIndex); // Fallback to local index
            }

            // Add bone weights to vertices using the global bone index
            for (uint32_t weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex) {
                const aiVertexWeight& weight = bone->mWeights[weightIndex];
                uint32_t vertexId = weight.mVertexId;

                if (vertexId < currentMesh.vertices_.size()) {
                    // Use the GLOBAL bone index instead of local mesh bone index
                    currentMesh.vertices_[vertexId].addBoneData(
                        static_cast<uint32_t>(globalBoneIndex), weight.mWeight);
                }
            }
        }

        // Second pass: normalize bone weights
        for (auto& vertex : currentMesh.vertices_) {
            vertex.normalizeBoneWeights();
        }
    }

    // print("Processed mesh {} with {} vertices and {} indices\n", meshIndex,
    //       currentMesh.vertices_.size(), currentMesh.indices_.size());
}

void ModelLoader::processMaterial(aiMaterial* material, const aiScene* scene,
                                  uint32_t materialIndex)
{
    Material& mat = model_.materials_[materialIndex];

    // Base color
    aiColor3D color;
    if (material->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
        mat.ubo_.baseColorFactor_ = vec4(color.r, color.g, color.b, 1.0f);
    }

    // Metallic factor
    float metallic;
    if (material->Get(AI_MATKEY_METALLIC_FACTOR, metallic) == AI_SUCCESS) {
        mat.ubo_.metallicFactor_ = metallic;
    }

    // Roughness factor
    float roughness;
    if (material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS) {
        mat.ubo_.roughness_ = roughness;
    }

    // Emissive factor
    aiColor3D emissive;
    if (material->Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == AI_SUCCESS) {
        mat.ubo_.emissiveFactor_ = vec4(emissive.r, emissive.g, emissive.b, 1.0f);
    }

    auto getTextureIndex = [this](const string& textureName, bool sRGB) -> int {
        auto it = std::find(model_.textureFilenames_.begin(), model_.textureFilenames_.end(),
                            textureName);
        if (it != model_.textureFilenames_.end()) {
            return static_cast<int>(std::distance(model_.textureFilenames_.begin(), it));
        } else {
            model_.textureFilenames_.push_back(textureName);
            model_.textureSRgb_.push_back(sRGB); // Store sRGB flag
            assert(model_.textureFilenames_.size() == model_.textureSRgb_.size());
            return static_cast<int>(model_.textureFilenames_.size() - 1);
        }
    };

    // Load textures
    aiString texturePath;

    if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS) {
        mat.ubo_.baseColorTextureIndex_ = getTextureIndex(texturePath.C_Str(), true);
    }

    if (material->GetTexture(aiTextureType_GLTF_METALLIC_ROUGHNESS, 0, &texturePath) ==
        AI_SUCCESS) {
        mat.ubo_.metallicRoughnessTextureIndex_ = getTextureIndex(texturePath.C_Str(), false);
    } else if (material->GetTexture(aiTextureType_SPECULAR, 0, &texturePath) == AI_SUCCESS) {
        mat.ubo_.metallicRoughnessTextureIndex_ = getTextureIndex(texturePath.C_Str(), false);
        // for Bistro model
    }

    if (material->GetTexture(aiTextureType_NORMALS, 0, &texturePath) == AI_SUCCESS) {
        mat.ubo_.normalTextureIndex_ = getTextureIndex(texturePath.C_Str(), false);
    }
    if (material->GetTexture(aiTextureType_LIGHTMAP, 0, &texturePath) == AI_SUCCESS) {
        mat.ubo_.occlusionTextureIndex_ = getTextureIndex(texturePath.C_Str(), false);
    }
    if (material->GetTexture(aiTextureType_EMISSIVE, 0, &texturePath) == AI_SUCCESS) {
        mat.ubo_.emissiveTextureIndex_ = getTextureIndex(texturePath.C_Str(), false);
    }

    // print("Processed materialIndex {}:\n", materialIndex);
    // print("  Base color: {}\n", glm::to_string(mat.baseColorFactor_));
    // print("  Metallic factor: {}\n", mat.metallicFactor_);
    // print("  Roughness factor: {}\n", mat.roughnessFactor_);
    // print("  Emissive factor: {}\n", glm::to_string(mat.emissiveFactor_));
    // print("  Base color texture: {}\n", mat.baseColorTexture_ ? "Loaded" : "None");
    // print("  MetallicRoughness texture: {}\n", mat.metallicRoughnessTexture_ ? "Loaded" :
    // "None"); print("  Normal texture: {}\n", mat.normalTexture_ ? "Loaded" : "None"); print("
    // Occlusion texture: {}\n", mat.occlusionTexture_ ? "Loaded" : "None"); print("  Emissive
    // texture: {}\n", mat.emissiveTexture_ ? "Loaded" : "None");
}

void ModelLoader::processMaterialBistro(aiMaterial* aiMat, const aiScene* scene,
                                        uint32_t materialIndex)
{
    Material& mat = model_.materials_[materialIndex];

    // Read parameters
    {
        aiColor4D Color;

        if (aiGetMaterialColor(aiMat, AI_MATKEY_COLOR_AMBIENT, &Color) == AI_SUCCESS) {
            mat.ubo_.emissiveFactor_ = {Color.r, Color.g, Color.b, Color.a};
            if (mat.ubo_.emissiveFactor_.w > 1.0f)
                mat.ubo_.emissiveFactor_.w = 1.0f;
        }
        if (aiGetMaterialColor(aiMat, AI_MATKEY_COLOR_DIFFUSE, &Color) == AI_SUCCESS) {
            mat.ubo_.baseColorFactor_ = {Color.r, Color.g, Color.b, Color.a};
            if (mat.ubo_.baseColorFactor_.w > 1.0f)
                mat.ubo_.baseColorFactor_.w = 1.0f;
        }
        if (aiGetMaterialColor(aiMat, AI_MATKEY_COLOR_EMISSIVE, &Color) == AI_SUCCESS) {
            mat.ubo_.emissiveFactor_ += vec4(Color.r, Color.g, Color.b, Color.a);
            if (mat.ubo_.emissiveFactor_.w > 1.0f)
                mat.ubo_.emissiveFactor_.w = 1.0f;
        }

        const float opaquenessThreshold = 0.05f;
        float Opacity = 1.0f;

        if (aiGetMaterialFloat(aiMat, AI_MATKEY_OPACITY, &Opacity) == AI_SUCCESS) {
            mat.ubo_.transparencyFactor_ = glm::clamp(1.0f - Opacity, 0.0f, 1.0f);
            if (mat.ubo_.transparencyFactor_ >= 1.0f - opaquenessThreshold)
                mat.ubo_.transparencyFactor_ = 0.0f;
        }

        if (aiGetMaterialColor(aiMat, AI_MATKEY_COLOR_TRANSPARENT, &Color) == AI_SUCCESS) {
            const float Opacity = std::max(std::max(Color.r, Color.g), Color.b);
            mat.ubo_.transparencyFactor_ = glm::clamp(Opacity, 0.0f, 1.0f);
            if (mat.ubo_.transparencyFactor_ >= 1.0f - opaquenessThreshold)
                mat.ubo_.transparencyFactor_ = 0.0f;
            mat.ubo_.discardAlpha_ = 0.5f;
        }

        float tmp = 1.0f;
        if (aiGetMaterialFloat(aiMat, AI_MATKEY_METALLIC_FACTOR, &tmp) == AI_SUCCESS)
            mat.ubo_.metallicFactor_ = tmp;

        if (aiGetMaterialFloat(aiMat, AI_MATKEY_ROUGHNESS_FACTOR, &tmp) == AI_SUCCESS)
            mat.ubo_.roughness_ = tmp;
    }

    // Load textures
    {
        // 안내: Bistro 모델의 텍스쳐 경로에는 앞에 "..\\"가 덧붙어 있어서 나중에 제거합니다.
        auto getTextureIndex = [this](string textureName, bool sRGB) -> int {
            filesystem::path fullPath("dummy/" + string(textureName));
            textureName = fullPath.lexically_normal().string();
            // printLog("Texture filename: {}", textureName);
            auto it = std::find(model_.textureFilenames_.begin(), model_.textureFilenames_.end(),
                                textureName);
            if (it != model_.textureFilenames_.end()) {
                return static_cast<int>(std::distance(model_.textureFilenames_.begin(), it));
            } else {
                model_.textureFilenames_.push_back(textureName);
                model_.textureSRgb_.push_back(sRGB); // Store sRGB flag
                assert(model_.textureFilenames_.size() == model_.textureSRgb_.size());
                return static_cast<int>(model_.textureFilenames_.size() - 1);
            }
        };

        aiString texturePath;

        if (aiMat->GetTexture(aiTextureType_EMISSIVE, 0, &texturePath) == AI_SUCCESS) {
            mat.ubo_.emissiveTextureIndex_ = getTextureIndex(texturePath.C_Str(), false);
        }

        if (aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS) {
            mat.ubo_.baseColorTextureIndex_ = getTextureIndex(texturePath.C_Str(), true);
            const std::string albedoMap = std::string(texturePath.C_Str());
            if (albedoMap.find("grey_30") != albedoMap.npos)
                mat.flags_ |= Material::sTransparent;
        }

        if (aiMat->GetTexture(aiTextureType_NORMALS, 0, &texturePath) == AI_SUCCESS) {
            mat.ubo_.normalTextureIndex_ = getTextureIndex(texturePath.C_Str(), false);
        } else if (aiMat->GetTexture(aiTextureType_HEIGHT, 0, &texturePath) == AI_SUCCESS) {
            mat.ubo_.normalTextureIndex_ = getTextureIndex(texturePath.C_Str(), false);
        }

        if (aiMat->GetTexture(aiTextureType_OPACITY, 0, &texturePath) == AI_SUCCESS) {
            mat.ubo_.opacityTextureIndex_ = getTextureIndex(texturePath.C_Str(), false);
            mat.ubo_.discardAlpha_ = 0.5f;
        }

        // TODO: 필요하면 PBR (Metallic-Roughness) 텍스처도 추가로 로드합니다.
    }

    // 기타 경험적으로 필요한 것들
    {
        aiString Name;
        std::string materialName;
        if (aiGetMaterialString(aiMat, AI_MATKEY_NAME, &Name) == AI_SUCCESS) {
            materialName = Name.C_Str();
        }

        mat.name_ = materialName;

        auto name = [&materialName](const char* substr) -> bool {
            return materialName.find(substr) != std::string::npos;
        };
        if (name("MASTER_Glass_Clean") || name("MenuSign_02_Glass") || name("Vespa_Headlight")) {
            mat.ubo_.discardAlpha_ = 0.75f;
            mat.ubo_.transparencyFactor_ = 0.2f;
            mat.flags_ |= Material::sTransparent;
        } else if (name("MASTER_Glass_Exterior") || name("MASTER_Focus_Glass")) {
            mat.ubo_.discardAlpha_ = 0.75f;
            mat.ubo_.transparencyFactor_ = 0.3f;
            mat.flags_ |= Material::sTransparent;
        } else if (name("MASTER_Frosted_Glass") || name("MASTER_Interior_01_Frozen_Glass")) {
            mat.ubo_.discardAlpha_ = 0.75f;
            mat.ubo_.transparencyFactor_ = 0.2f;
            mat.flags_ |= Material::sTransparent;
        } else if (name("Streetlight_Glass")) {
            mat.ubo_.discardAlpha_ = 0.75f;
            mat.ubo_.transparencyFactor_ = 0.15f;
            mat.ubo_.baseColorTextureIndex_ = -1;
            mat.flags_ |= Material::sTransparent;
        } else if (name("Paris_LiquorBottle_01_Glass_Wine")) {
            mat.ubo_.discardAlpha_ = 0.56f;
            mat.ubo_.transparencyFactor_ = 0.35f;
            mat.flags_ |= Material::sTransparent;
        } else if (name("_Caps") || name("_Labels")) {
            // not transparent
        } else if (name("Paris_LiquorBottle_02_Glass")) {
            mat.ubo_.discardAlpha_ = 0.56f;
            mat.ubo_.transparencyFactor_ = 0.1f;
        } else if (name("Bottle")) {
            mat.ubo_.discardAlpha_ = 0.56f;
            mat.ubo_.transparencyFactor_ = 0.2f;
            mat.flags_ |= Material::sTransparent;
        } else if (name("Glass")) {
            mat.ubo_.discardAlpha_ = 0.56f;
            mat.ubo_.transparencyFactor_ = 0.1f;
            mat.flags_ |= Material::sTransparent;
        } else if (name("Metal")) {
            mat.ubo_.metallicFactor_ = 1.0f;
            mat.ubo_.roughness_ = 0.1f;
        }
    }
}

void ModelLoader::updateMatrices()
{
    if (model_.rootNode_) {
        model_.rootNode_->updateWorldMatrix();
    }
}

void ModelLoader::printVerticesAndIndices() const
{
    printLog("\nModel Vertices and Indices");
    printLog("  File: {}", directory_);
    printLog("  Total meshes: {}", model_.meshes_.size());
    printLog("  Total materials: {}", model_.materials_.size());
    printLog("  Model bounding box: min({}, {}, {}), max({}, {}, {})", model_.boundingBoxMin_.x,
             model_.boundingBoxMin_.y, model_.boundingBoxMin_.z, model_.boundingBoxMax_.x,
             model_.boundingBoxMax_.y, model_.boundingBoxMax_.z);

    return;

    for (size_t meshIdx = 0; meshIdx < model_.meshes_.size(); ++meshIdx) {
        const Mesh& mesh = model_.meshes_[meshIdx];

        printLog("  Mesh {}: vertices = {}, indices = {}, material = {}", meshIdx,
                 mesh.vertices_.size(), mesh.materialIndex_, mesh.indices_.size(),
                 mesh.materialIndex_);
        printLog("  Mesh bounding box: min({}, {}, {}), max({}, {}, {})", mesh.minBounds.x,
                 mesh.minBounds.y, mesh.minBounds.z, mesh.maxBounds.x, mesh.maxBounds.y,
                 mesh.maxBounds.z);

        // Print vertices (limit to first 10 to avoid spam)
        // size_t maxVertices = std::min(mesh.vertices_.size(), static_cast<size_t>(10));
        // print("Vertices (first {}):\n", maxVertices);
        // for (size_t i = 0; i < maxVertices; ++i) {
        //    const Vertex &v = mesh.vertices_[i];
        //    print("  [{}] pos: ({}, {}, {})\n", i, v.position.x, v.position.y, v.position.z);
        //    print("       normal: ({}, {}, {})\n", v.normal.x, v.normal.y, v.normal.z);
        //    print("       texCoord: ({}, {})\n", v.texCoord.x, v.texCoord.y);
        //    print("       tangent: ({}, {}, {})\n", v.tangent.x, v.tangent.y, v.tangent.z);
        //    print("       bitangent: ({}, {}, {})\n", v.bitangent.x, v.bitangent.y,
        //    v.bitangent.z);
        //}

        // if (mesh.vertices_.size() > maxVertices) {
        //     print("  ... and {} more vertices\n", mesh.vertices_.size() - maxVertices);
        // }

        //// Print indices (limit to first 30 to avoid spam)
        // size_t maxIndices = std::min(mesh.indices_.size(), static_cast<size_t>(30));
        // print("Indices (first {}):\n  ", maxIndices);
        // for (size_t i = 0; i < maxIndices; ++i) {
        //     print("{}", mesh.indices_[i]);
        //     if (i < maxIndices - 1)
        //         print(", ");
        //     if ((i + 1) % 10 == 0)
        //         print("\n  "); // New line every 10 indices
        // }
        // print("\n");

        // if (mesh.indices_.size() > maxIndices) {
        //     print("  ... and {} more indices\n", mesh.indices_.size() - maxIndices);
        // }

        //// Print triangles formed by first few indices
        // print("First few triangles:\n");
        // size_t maxTriangles = std::min(mesh.indices_.size() / 3, static_cast<size_t>(3));
        // for (size_t i = 0; i < maxTriangles; ++i) {
        //     size_t idx0 = mesh.indices_[i * 3 + 0];
        //     size_t idx1 = mesh.indices_[i * 3 + 1];
        //     size_t idx2 = mesh.indices_[i * 3 + 2];

        //    print("  Triangle {}: indices [{}, {}, {}]\n", i, idx0, idx1, idx2);

        //    if (idx0 < mesh.vertices_.size() && idx1 < mesh.vertices_.size() &&
        //        idx2 < mesh.vertices_.size()) {
        //        const Vertex &v0 = mesh.vertices_[idx0];
        //        const Vertex &v1 = mesh.vertices_[idx1];
        //        const Vertex &v2 = mesh.vertices_[idx2];

        //        print("    v0: ({}, {}, {})\n", v0.position.x, v0.position.y, v0.position.z);
        //        print("    v1: ({}, {}, {})\n", v1.position.x, v1.position.y, v1.position.z);
        //        print("    v2: ({}, {}, {})\n", v2.position.x, v2.position.y, v2.position.z);
        //    }
        //}
    }
}

void ModelLoader::debugWriteEmbeddedTextures() const
{
    const aiScene* scene = importer_.GetScene();
    if (!scene || scene->mNumTextures == 0) {
        printLog("No embedded textures found in the model");
        return;
    }

    printLog("Found {} embedded textures, writing to debug files...", scene->mNumTextures);

    // Create debug directory if it doesn't exist
    string debugDir = "debug_textures";
    filesystem::create_directories(debugDir);

    for (uint32_t i = 0; i < scene->mNumTextures; ++i) {
        const aiTexture* aiTex = scene->mTextures[i];

        string filename;
        if (aiTex->mHeight == 0) {
            // Compressed texture data (PNG, JPG, etc.)
            // Try to determine format from the format hint
            string formatHint = string(aiTex->achFormatHint);
            if (formatHint.empty() || formatHint == "\0\0\0\0") {
                // Try to detect format from data header
                const unsigned char* data = reinterpret_cast<const unsigned char*>(aiTex->pcData);
                if (data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47) {
                    formatHint = "png";
                } else if (data[0] == 0xFF && data[1] == 0xD8) {
                    formatHint = "jpg";
                } else {
                    formatHint = "bin"; // Unknown format
                }
            }

            filename = debugDir + "/embedded_texture_" + to_string(i) + "." + formatHint;

            // Write compressed data directly to file
            FILE* file = fopen(filename.c_str(), "wb");
            if (file) {
                fwrite(aiTex->pcData, 1, aiTex->mWidth, file);
                fclose(file);
                printLog("Wrote compressed texture {}: {} ({} bytes)", i, filename, aiTex->mWidth);
            } else {
                printLog("Failed to write compressed texture {}: {}", i, filename);
            }
        } else {
            // Uncompressed RGBA texture data
            filename = debugDir + "/embedded_texture_" + to_string(i) + ".png";

            // Convert aiTexel to RGBA8
            vector<unsigned char> rgba8Data(aiTex->mWidth * aiTex->mHeight * 4);
            for (uint32_t j = 0; j < static_cast<uint32_t>(aiTex->mWidth * aiTex->mHeight); ++j) {
                rgba8Data[j * 4 + 0] = static_cast<unsigned char>(aiTex->pcData[j].r * 255);
                rgba8Data[j * 4 + 1] = static_cast<unsigned char>(aiTex->pcData[j].g * 255);
                rgba8Data[j * 4 + 2] = static_cast<unsigned char>(aiTex->pcData[j].b * 255);
                rgba8Data[j * 4 + 3] = static_cast<unsigned char>(aiTex->pcData[j].a * 255);
            }

            // Write as PNG
            if (stbi_write_png(filename.c_str(), aiTex->mWidth, aiTex->mHeight, 4, rgba8Data.data(),
                               aiTex->mWidth * 4)) {
                printLog("Wrote uncompressed texture {}: {} ({}x{})", i, filename, aiTex->mWidth,
                         aiTex->mHeight);
            } else {
                printLog("Failed to write uncompressed texture {}: {}", i, filename);
            }
        }
    }

    printLog("Finished writing embedded textures to {} directory", debugDir);
}

void ModelLoader::optimizeMeshesBistro()
{
    auto& meshes = model_.meshes_;
    auto& materials = model_.materials_;

    // 주의: mesh를 합친 후에는 그래프 구조에서도 합쳐진 것들을 반영시켜줘야 합니다.
    //      예: 그래프의 노드끼리도 합치기

    vector<string> materialNamesToMerge = {"Foliage_Linde_Tree_Large_Orange_Leaves",
                                           "Foliage_Linde_Tree_Large_Green_Leaves",
                                           "Foliage_Linde_Tree_Large_Trunk"};

    uint32_t totalMergedMeshes = 0;

    for (const auto& name : materialNamesToMerge) {

        vector<uint32_t> meshIndicesToMerge;
        for (uint32_t i = 0; i < meshes.size(); ++i) {
            if (materials[meshes[i].materialIndex_].name_ == name &&
                meshes[i].noTextureCoords == false) {
                meshIndicesToMerge.push_back(i);
            }
        }

        if (meshIndicesToMerge.size() < 2) {
            printLog("No meshes found with material name '{}', skipping merge.", name);
            continue;
        }

        Mesh& firstMesh = meshes[meshIndicesToMerge[0]];
        // keep name_ of the first mesh

        // Merge vertices from all meshes into the first mesh
        uint32_t baseVertexCount = static_cast<uint32_t>(firstMesh.vertices_.size());

        for (size_t i = 1; i < meshIndicesToMerge.size(); ++i) {
            uint32_t meshIndex = meshIndicesToMerge[i];
            Mesh& otherMesh = meshes[meshIndex];

            // Append vertices from other mesh
            firstMesh.vertices_.insert(firstMesh.vertices_.end(), otherMesh.vertices_.begin(),
                                       otherMesh.vertices_.end());

            // Append indices from other mesh, adjusting them by the base vertex count
            uint32_t currentBaseVertexCount = baseVertexCount;
            for (uint32_t index : otherMesh.indices_) {
                firstMesh.indices_.push_back(index + currentBaseVertexCount);
            }

            // Update base vertex count for next iteration
            baseVertexCount += static_cast<uint32_t>(otherMesh.vertices_.size());
        }

        // Mark merged meshes for removal (except the first one)
        for (size_t i = 1; i < meshIndicesToMerge.size(); ++i) {
            uint32_t meshIndex = meshIndicesToMerge[i];
            meshes[meshIndex].vertices_.clear();
            meshes[meshIndex].indices_.clear();
        }

        totalMergedMeshes += static_cast<uint32_t>(meshIndicesToMerge.size() - 1);

        firstMesh.calculateBounds();

        printLog("Merged {} meshes with material '{}' into mesh {}", meshIndicesToMerge.size(),
                 name, meshIndicesToMerge[0]);
    }

    // TODO: update following not to use copy assignment operator
    // Remove empty meshes (ones that were merged)
    auto writeIter = meshes.begin();
    for (auto readIter = meshes.begin(); readIter != meshes.end(); ++readIter) {
        if (!readIter->vertices_.empty()) {
            if (writeIter != readIter) {
                *writeIter = std::move(*readIter);
            }
            ++writeIter;
        }
    }
    meshes.erase(writeIter, meshes.end());

    printLog("Successfully optimized Bistro model");
    printLog("  Merged {} meshes", totalMergedMeshes);
    printLog("  Meshes after optimization: {}", meshes.size());
    printLog("  Materials: {}", materials.size());
}

void ModelLoader::processAnimations(const aiScene* scene)
{
    if (!scene || scene->mNumAnimations == 0) {
        printLog("No animations found in the model");
        return;
    }

    printLog("Processing animations in Model...");
    printLog("  Scene has {} animations", scene->mNumAnimations);

    // Load animation data using our Animation class
    // The Animation system will calculate its own global inverse transform in loadFromScene
    model_.animation_->loadFromScene(scene);

    if (model_.animation_->hasAnimations()) {
        printLog("Successfully loaded {} animation clips", model_.animation_->getAnimationCount());
        printLog("  Current animation: '{}'", model_.animation_->getCurrentAnimationName());
        printLog("  Duration: {:.2f} seconds", model_.animation_->getDuration());
    }

    if (model_.animation_->hasBones()) {
        printLog("Successfully loaded {} bones for skeletal animation",
                 model_.animation_->getBoneCount());
    }
}

void ModelLoader::processBones(const aiScene* scene)
{
    if (!scene)
        return;

    // Check if any mesh has bones
    bool hasBones = false;
    for (uint32_t i = 0; i < scene->mNumMeshes; ++i) {
        if (scene->mMeshes[i]->HasBones()) {
            hasBones = true;
            break;
        }
    }

    if (!hasBones) {
        printLog("No bones found in any mesh");
        return;
    }

    uint32_t totalBones = 0;
    for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        const aiMesh* mesh = scene->mMeshes[meshIndex];
        if (mesh->HasBones()) {
            totalBones += mesh->mNumBones;
        }
    }

    printLog("Total bones across all meshes: {}", totalBones);
}

} // namespace hlab