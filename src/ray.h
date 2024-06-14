
#pragma once
#include "vector.h"

class Ray {
public:
    Vector3 origin;
    Vector3 direction;

    Ray(const Vector3& origin, const Vector3& direction);

    Vector3 point_at_parameter(float t) const;
};

