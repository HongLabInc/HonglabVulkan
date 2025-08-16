#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace hlab {

class Camera
{
  public:
    enum CameraType { lookat, firstperson };

  public:
    float fov;
    float znear, zfar;
    CameraType type = CameraType::firstperson;

    void updateViewMatrix();

  public:
    glm::vec3 rotation = glm::vec3(-1.888507, -0.764950, -0.725987); // For Bistro model
    glm::vec3 position = glm::vec3(6.000000, -62.000000, 0.000000);
    glm::vec3 viewPos = glm::vec3(1.888507, -0.764950, 0.725987);

    float rotationSpeed = 0.1f;
    float movementSpeed = 10.0f;

    bool updated = true;

    struct
    {
        glm::mat4 perspective;
        glm::mat4 view;
    } matrices;

    struct
    {
        bool left = false;
        bool right = false;
        bool forward = false;
        bool backward = false;
        bool up = false;
        bool down = false;
    } keys;

    Camera();
    ~Camera();

    bool moving() const;
    float getNearClip() const;
    float getFarClip() const;
    CameraType getType() const;
    void setType(CameraType type);

    void setPerspective(float fov, float aspect, float znear, float zfar);
    void setPosition(glm::vec3 position);
    void setRotation(glm::vec3 rotation);
    void setViewPos(glm::vec3 viewPos);
    void rotate(glm::vec3 delta);
    void setTranslation(glm::vec3 translation);
    void translate(glm::vec3 delta);
    void setRotationSpeed(float rotationSpeed);
    void setMovementSpeed(float movementSpeed);
    void update(float deltaTime);
};

} // namespace hlab