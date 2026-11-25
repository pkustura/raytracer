#pragma once
#include <cstdint>
#include <vector>
#include "camera.h"
#include "color.h"
#include "light.h"
#include "material.h"
#include "shape.h"

Vector3 reflect(const Vector3& I, const Vector3& N);
bool refract(const Vector3& I, const Vector3& N, float ior, Vector3& refracted);

// Phong-style direct lighting (ambient + diffuse + specular).
Color shade(const Material& material, const Vector3& normal, const Vector3& viewDir,
            const Vector3& lightDir, const Vector3& lightColor);

// Traces a ray through the scene, following mirror reflections and glass
// reflection/refraction up to a fixed depth.
Color ray_color(const Ray& ray, const std::vector<Shape*>& shapes, const Light& light, int depth = 0);

// Renders one frame into `image` (row-major, bottom row first) using `threads`
// worker threads (0 = one per hardware thread).
// Returns the number of rays traced, including secondary rays.
uint64_t render_frame(const Camera& camera, const std::vector<Shape*>& shapes, const Light& light,
                      int width, int height, std::vector<Color>& image, int threads = 0);
