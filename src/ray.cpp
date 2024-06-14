#include "ray.h"

Ray::Ray(const Vector3& origin, const Vector3& direction)
    : origin(origin), direction(direction) {}

Vector3 Ray::point_at_parameter(float t) const {
    return origin + direction * t;
}
