#include "ModelNode.h"
#include "Material.h"
#include "Mesh.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>

namespace hlab {

void ModelNode::updateLocalMatrix()
{
    mat4 translationMatrix = translate(mat4(1.0f), translation);
    mat4 rotationMatrix = mat4_cast(rotation);
    mat4 scaleMatrix = glm::scale(mat4(1.0f), scale);

    localMatrix = translationMatrix * rotationMatrix * scaleMatrix;
}

void ModelNode::updateWorldMatrix(const mat4 &parentMatrix)
{
    worldMatrix = parentMatrix * localMatrix;

    for (auto &child : children) {
        child->updateWorldMatrix(worldMatrix);
    }
}

ModelNode *ModelNode::findNode(const std::string &name)
{
    if (this->name == name) {
        return this;
    }

    for (auto &child : children) {
        ModelNode *result = child->findNode(name);
        if (result) {
            return result;
        }
    }

    return nullptr;
}

} // namespace hlab
