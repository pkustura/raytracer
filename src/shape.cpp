#include "shape.h"
#include <cmath>

// Plane intersection
bool Plane::intersect(const Ray& ray, float& t) const {
    float denom = normal.dot(ray.direction);
    if (std::abs(denom) > 1e-6) {
        Vector3 p0l0 = point - ray.origin;
        t = p0l0.dot(normal) / denom;
        return (t >= 0);
    }
    return false;
}

Vector3 Plane::get_normal(const Vector3& point) const {
    return normal;
}

// Sphere intersection
bool Sphere::intersect(const Ray& ray, float& t) const {
    Vector3 oc = ray.origin - center;
    float a = ray.direction.dot(ray.direction);
    float b = 2.0 * oc.dot(ray.direction);
    float c = oc.dot(oc) - radius * radius;
    float discriminant = b * b - 4 * a * c;

    if (discriminant > 0) {
        t = (-b - std::sqrt(discriminant)) / (2.0 * a);
        if (t > 0) return true;
        t = (-b + std::sqrt(discriminant)) / (2.0 * a);
        if (t > 0) return true;
    }
    return false;
}

Vector3 Sphere::get_normal(const Vector3& point) const {
    return (point - center).normalized();
}

