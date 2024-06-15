#pragma once
#include "color.h"

class Material {
public:
    Color diffuse;
    Color specular;
    float shininess;

    Material(const Color& diffuse, const Color& specular, float shininess);
};
