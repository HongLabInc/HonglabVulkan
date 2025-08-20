#pragma once

#include <string>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp> // For glm::make_mat4

namespace hlab {

using namespace std;
using namespace glm;

class Model;
class ModelNode;

class ModelLoader
{
  public:
    ModelLoader(Model& model);
    void loadFromModelFile(const string& modelFilename, bool readBistroObj);
    void loadFromCache(const string& cacheFilename);
    void writeToCache(const string& cacheFilename);

    void processNode(aiNode* node, const aiScene* scene, ModelNode* parent = nullptr);
    void processMesh(aiMesh* mesh, const aiScene* scene, uint32_t meshIndex);
    void processMaterial(aiMaterial* material, const aiScene* scene, uint32_t materialIndex);
    void processMaterialBistro(aiMaterial* material, const aiScene* scene, uint32_t materialIndex);
    void processAnimations(const aiScene* scene);
    void processBones(const aiScene* scene);

    void debugWriteEmbeddedTextures() const;
    void optimizeMeshesBistro();
    void printVerticesAndIndices() const;
    void updateMatrices();

  private:
    Model& model_;

    Assimp::Importer importer_;
    string directory_;
};

} // namespace hlab