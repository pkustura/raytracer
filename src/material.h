#pragma once
#include "color.h"

class Material {
public:
    Color ambient;
    Color diffuse;
    Color specular;
    Color mirror_reflectance;
    float transparency;
    float shininess;
    float ior; // index of refraction
               

    Material(const Color& ambient, const Color& diffuse, const Color& specular, const Color& mirror_reflectance, float transparency, float shininess, float ior);

    // methods for different types of shading can be added here
};

class DiffuseMaterial : public Material {
public:
    DiffuseMaterial(const Color& ambient, const Color& diffuse);
};

class PhongMaterial : public Material {
public:
    PhongMaterial(const Color& ambient, const Color& diffuse, const Color& specular, float shininess);
};

class MirrorMaterial : public Material {
public:
    MirrorMaterial(const Color& ambient, const Color& mirror_reflectance);
};

class GlassMaterial : public Material {
public:
    GlassMaterial(const Color& ambient, const Color& mirror_reflectance, float transparency, float ior);
};
