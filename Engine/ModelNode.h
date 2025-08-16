#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

using namespace glm;
using namespace std;

namespace hlab {

class Mesh;
class Material;

class ModelNode
{
  public:
    string name;
    mat4 localMatrix = mat4(1.0f);
    mat4 worldMatrix = mat4(1.0f);

    // References to meshes in the parent Model
    vector<uint32_t> meshIndices;
    vector<unique_ptr<ModelNode>> children;
    ModelNode *parent = nullptr;

    vec3 translation = vec3(0.0f);
    quat rotation = quat(1.0f, 0.0f, 0.0f, 0.0f);
    vec3 scale = vec3(1.0f);

    void updateLocalMatrix();
    void updateWorldMatrix(const mat4 &parentMatrix = mat4(1.0f));
    ModelNode *findNode(const string &name);

  private:
};

} // namespace hlab
