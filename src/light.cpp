#include "light.h"

Light::Light(Vector3 position, Vector3 intensity)
    : position(position), intensity(intensity) {}

Vector3 Light::get_position() const {
    return position;
}

Vector3 Light::get_intensity() const {
    return intensity;
}

