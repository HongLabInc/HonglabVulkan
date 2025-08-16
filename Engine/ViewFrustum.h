#pragma once

#include <array>
#include <glm/glm.hpp>

namespace hlab {

struct Plane
{
    glm::vec3 normal;
    float distance;

    Plane() = default;
    Plane(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3);
    Plane(const glm::vec3& normal, const glm::vec3& point);

    float distanceToPoint(const glm::vec3& point) const;
};

struct AABB
{
    glm::vec3 min;
    glm::vec3 max;

    AABB() = default;
    AABB(const glm::vec3& min, const glm::vec3& max) : min(min), max(max)
    {
    }

    glm::vec3 getCenter() const
    {
        return (min + max) * 0.5f;
    }
    glm::vec3 getExtents() const
    {
        return (max - min) * 0.5f;
    }

    // Transform AABB by matrix
    AABB transform(const glm::mat4& matrix) const;
};

class ViewFrustum
{
  public:
    enum PlaneIndex { sLEFT = 0, sRIGHT = 1, sBOTTOM = 2, sTOP = 3, sNEAR = 4, sFAR = 5 };

    ViewFrustum() = default;

    // Extract frustum planes from view-projection matrix
    void extractFromViewProjection(const glm::mat4& viewProjection);

    // Test if AABB is inside or intersecting frustum
    bool intersects(const AABB& aabb) const;

    // Test if point is inside frustum
    bool contains(const glm::vec3& point) const;

  private:
    std::array<Plane, 6> planes_{};
};

} // namespace hlab