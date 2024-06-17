#include "material.h"

Material::Material(const Color& ambient, const Color& diffuse, const Color& specular, const Color& mirror_reflectance, float transparency, float shininess, float ior)
    : ambient(ambient), diffuse(diffuse), specular(specular), mirror_reflectance(mirror_reflectance), transparency(transparency), shininess(shininess), ior(ior) {}

DiffuseMaterial::DiffuseMaterial(const Color& ambient, const Color& diffuse)
    : Material(ambient, diffuse, Color(0, 0, 0), Color(0,0,0), 0.0f, 0.0f, 0.0f) {}

PhongMaterial::PhongMaterial(const Color& ambient, const Color& diffuse, const Color& specular, float shininess)
    : Material(ambient, diffuse, specular, Color(0, 0, 0), 0.0f, shininess, 1.0f) {}

MirrorMaterial::MirrorMaterial(const Color& ambient, const Color& mirror_reflectance)
    : Material(ambient, Color(0, 0, 0), Color(0, 0, 0), mirror_reflectance, 0.0f, 0.0f, 0.0f) {}

GlassMaterial::GlassMaterial(const Color& ambient, const Color& mirror_reflectance, float transparency, float ior)
    : Material(ambient, Color(0, 0, 0), Color(0, 0, 0), mirror_reflectance, transparency, 0.0f, ior) {}

