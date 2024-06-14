#pragma once
#include "ray.h"
#include "vector.h"
#include "material.h"

class Shape {
public:
    Material material;

    Shape(const Material& material) : material(material) {}
    virtual ~Shape() = default;  // virtual destructor

    virtual bool intersect(const Ray& ray, float& t) const = 0;
    virtual Vector3 get_normal(const Vector3& point) const = 0;
};

class Plane : public Shape {
public:
    Vector3 point;
    Vector3 normal;

    Plane(const Vector3& point, const Vector3& normal, const Material& material)
        : Shape(material), point(point), normal(normal) {}

    bool intersect(const Ray& ray, float& t) const override;
    Vector3 get_normal(const Vector3& point) const override;
};

class Sphere : public Shape {
public:
    Vector3 center;
    float radius;

    Sphere(const Vector3& center, float radius, const Material& material)
        : Shape(material), center(center), radius(radius) {}

    bool intersect(const Ray& ray, float& t) const override;
    Vector3 get_normal(const Vector3& point) const override;
};

