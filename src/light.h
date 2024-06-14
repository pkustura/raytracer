#pragma once
#include "vector.h"

class Light {
public:
    Vector3 position;
    Vector3 intensity;

    Light(Vector3 position, Vector3 intensity);

    Vector3 get_position() const;
    Vector3 get_intensity() const;
};

