#include "Camera.h"
#include <cmath>

namespace hlab {

using namespace glm;

Camera::Camera() : fov(45.0f), znear(0.01f), zfar(1000.0f), type(CameraType::lookat)
{
    matrices.perspective = glm::mat4(1.0f);
    matrices.view = glm::mat4(1.0f);
}

Camera::~Camera()
{
}

void Camera::updateViewMatrix()
{
    glm::mat4 currentMatrix = matrices.view;

    glm::mat4 rotM = glm::mat4(1.0f);
    glm::mat4 transM;

    rotM = glm::rotate(rotM, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    rotM = glm::rotate(rotM, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    rotM = glm::rotate(rotM, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

    glm::vec3 translation = position;

    transM = glm::translate(glm::mat4(1.0f), translation);

    if (type == CameraType::firstperson) {
        matrices.view = rotM * transM;
    } else {
        matrices.view = transM * rotM;
    }

    viewPos = glm::vec4(position, 0.0f) * glm::vec4(-1.0f, 1.0f, -1.0f, 1.0f);

    if (matrices.view != currentMatrix) {
        updated = true;
    }
}

bool Camera::moving() const
{
    return keys.left || keys.right || keys.forward || keys.backward || keys.up || keys.down;
}

float Camera::getNearClip() const
{
    return znear;
}

float Camera::getFarClip() const
{
    return zfar;
}

Camera::CameraType Camera::getType() const
{
    return type;
}

void Camera::setType(CameraType type)
{
    this->type = type;
}

void Camera::setPerspective(float fov, float aspect, float znear, float zfar)
{
    glm::mat4 currentMatrix = matrices.perspective;
    this->fov = fov;
    this->znear = znear;
    this->zfar = zfar;
    matrices.perspective = glm::perspectiveRH_ZO(glm::radians(fov), aspect, znear, zfar);
    matrices.perspective[1][1] *= -1.0f;
    // 안내: 뷰스페이스에서 y 방향이 아래를 향하는 것이 직관적이지 않을 수 있어서
    //      여기서는 P 행렬에서 뒤집었습니다.
    if (matrices.perspective != currentMatrix) {
        updated = true;
    }
}

void Camera::setPosition(glm::vec3 position)
{
    this->position = position;
    updateViewMatrix();
}

void Camera::setRotation(glm::vec3 rotation)
{
    this->rotation = rotation;
    updateViewMatrix();
}

void Camera::setViewPos(glm::vec3 viewPos)
{
    this->viewPos = viewPos;
    updateViewMatrix();
}

void Camera::rotate(glm::vec3 delta)
{
    this->rotation += delta;
    updateViewMatrix();
}

void Camera::setTranslation(glm::vec3 translation)
{
    this->position = translation;
    updateViewMatrix();
}

void Camera::translate(glm::vec3 delta)
{
    this->position += delta;
    updateViewMatrix();
}

void Camera::setRotationSpeed(float rotationSpeed)
{
    this->rotationSpeed = rotationSpeed;
}

void Camera::setMovementSpeed(float movementSpeed)
{
    this->movementSpeed = movementSpeed;
}

void Camera::update(float deltaTime)
{
    const vec3 upDirection = normalize(vec3(0.0f, 1.0f, 0.0f));

    updated = false;
    if (type == CameraType::firstperson) {
        if (moving()) {
            glm::vec3 camFront;
            camFront.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y));
            camFront.y = sin(glm::radians(rotation.x));
            camFront.z = cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y));
            camFront = glm::normalize(camFront);

            float moveSpeed = deltaTime * movementSpeed;

            if (keys.forward)
                position += camFront * moveSpeed;
            if (keys.backward)
                position -= camFront * moveSpeed;
            if (keys.left)
                position -= glm::normalize(glm::cross(camFront, upDirection)) * moveSpeed;
            if (keys.right)
                position += glm::normalize(glm::cross(camFront, upDirection)) * moveSpeed;
            if (keys.up) {
                position += upDirection * moveSpeed;
            }
            if (keys.down) {
                position -= upDirection * moveSpeed;
            }
        }
    }
    updateViewMatrix();
}

} // namespace hlab