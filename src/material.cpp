#include "material.h"

Material::Material(const Color& diffuse, const Color& specular, float shininess)
    : diffuse(diffuse), specular(specular), shininess(shininess) {}

