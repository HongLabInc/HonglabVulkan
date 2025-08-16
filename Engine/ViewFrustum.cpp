#include "ViewFrustum.h"
#include <algorithm>

namespace hlab {

Plane::Plane(const glm::vec3 &p1, const glm::vec3 &p2, const glm::vec3 &p3)
{
    glm::vec3 v1 = p2 - p1;
    glm::vec3 v2 = p3 - p1;
    normal = glm::normalize(glm::cross(v1, v2));
    distance = -glm::dot(normal, p1);
}

Plane::Plane(const glm::vec3 &normal, const glm::vec3 &point) : normal(glm::normalize(normal))
{
    distance = -glm::dot(this->normal, point);
}

float Plane::distanceToPoint(const glm::vec3 &point) const
{
    return glm::dot(normal, point) + distance;
}

AABB AABB::transform(const glm::mat4 &matrix) const
{
    // Transform all 8 corners and find new min/max
    glm::vec3 corners[8] = {{min.x, min.y, min.z}, {max.x, min.y, min.z}, {min.x, max.y, min.z},
                            {max.x, max.y, min.z}, {min.x, min.y, max.z}, {max.x, min.y, max.z},
                            {min.x, max.y, max.z}, {max.x, max.y, max.z}};

    glm::vec3 newMin(FLT_MAX);
    glm::vec3 newMax(-FLT_MAX);

    for (int i = 0; i < 8; ++i) {
        glm::vec4 transformed = matrix * glm::vec4(corners[i], 1.0f);
        glm::vec3 point = glm::vec3(transformed) / transformed.w;

        newMin = glm::min(newMin, point);
        newMax = glm::max(newMax, point);
    }

    return AABB(newMin, newMax);
}

void ViewFrustum::extractFromViewProjection(const glm::mat4 &viewProjection)
{
    // Extract frustum planes from view-projection matrix
    // Use row-major indexing since GLM uses column-major storage
    const float *m = &viewProjection[0][0];

    // Left plane: m[3] + m[0], m[7] + m[4], m[11] + m[8], m[15] + m[12]
    planes_[sLEFT].normal.x = m[3] + m[0];
    planes_[sLEFT].normal.y = m[7] + m[4];
    planes_[sLEFT].normal.z = m[11] + m[8];
    planes_[sLEFT].distance = m[15] + m[12];

    // Right plane: m[3] - m[0], m[7] - m[4], m[11] - m[8], m[15] - m[12]
    planes_[sRIGHT].normal.x = m[3] - m[0];
    planes_[sRIGHT].normal.y = m[7] - m[4];
    planes_[sRIGHT].normal.z = m[11] - m[8];
    planes_[sRIGHT].distance = m[15] - m[12];

    // Bottom plane: m[3] + m[1], m[7] + m[5], m[11] + m[9], m[15] + m[13]
    planes_[sBOTTOM].normal.x = m[3] + m[1];
    planes_[sBOTTOM].normal.y = m[7] + m[5];
    planes_[sBOTTOM].normal.z = m[11] + m[9];
    planes_[sBOTTOM].distance = m[15] + m[13];

    // Top plane: m[3] - m[1], m[7] - m[5], m[11] - m[9], m[15] - m[13]
    planes_[sTOP].normal.x = m[3] - m[1];
    planes_[sTOP].normal.y = m[7] - m[5];
    planes_[sTOP].normal.z = m[11] - m[9];
    planes_[sTOP].distance = m[15] - m[13];

    // Near plane: m[3] + m[2], m[7] + m[6], m[11] + m[10], m[15] + m[14]
    planes_[sNEAR].normal.x = m[3] + m[2];
    planes_[sNEAR].normal.y = m[7] + m[6];
    planes_[sNEAR].normal.z = m[11] + m[10];
    planes_[sNEAR].distance = m[15] + m[14];

    // Far plane: m[3] - m[2], m[7] - m[6], m[11] - m[10], m[15] - m[14]
    planes_[sFAR].normal.x = m[3] - m[2];
    planes_[sFAR].normal.y = m[7] - m[6];
    planes_[sFAR].normal.z = m[11] - m[10];
    planes_[sFAR].distance = m[15] - m[14];

    // Normalize all planes
    for (auto &plane : planes_) {
        float length = glm::length(plane.normal);
        if (length > 0.0f) {
            plane.normal /= length;
            plane.distance /= length;
        }
    }
}

bool ViewFrustum::intersects(const AABB &aabb) const
{
    for (const auto &plane : planes_) {
        // Find the positive vertex (farthest in direction of plane normal)
        glm::vec3 positiveVertex = aabb.min;
        if (plane.normal.x >= 0)
            positiveVertex.x = aabb.max.x;
        if (plane.normal.y >= 0)
            positiveVertex.y = aabb.max.y;
        if (plane.normal.z >= 0)
            positiveVertex.z = aabb.max.z;

        // If positive vertex is behind plane, AABB is completely outside
        if (plane.distanceToPoint(positiveVertex) < 0) {
            return false;
        }
    }
    return true; // AABB is either inside or intersecting
}

bool ViewFrustum::contains(const glm::vec3 &point) const
{
    for (const auto &plane : planes_) {
        if (plane.distanceToPoint(point) < 0) {
            return false;
        }
    }
    return true;
}

} // namespace hlab