#pragma once
#include <cstdint>
#include <vector>
#include "camera.h"
#include "color.h"
#include "light.h"
#include "material.h"
#include "scene.h"
#include "shape.h"

Vector3 reflect(const Vector3& I, const Vector3& N);
bool refract(const Vector3& I, const Vector3& N, float ior, Vector3& refracted);

// Phong-style direct lighting (ambient + diffuse + specular).
Color shade(const Material& material, const Vector3& normal, const Vector3& viewDir,
            const Vector3& lightDir, const Vector3& lightColor);

const int DEFAULT_MAX_DEPTH = 5;

// Traces a ray through the scene, following mirror reflections and glass
// reflection/refraction up to max_depth bounces.
Color ray_color(const Ray& ray, const std::vector<Shape*>& shapes, const Light& light,
                int depth = 0, int max_depth = DEFAULT_MAX_DEPTH);

// Renders one frame into `image` (row-major, bottom row first) using `threads`
// worker threads (0 = one per hardware thread).
// Returns the number of rays traced, including secondary rays.
uint64_t render_frame(const Camera& camera, const std::vector<Shape*>& shapes, const Light& light,
                      int width, int height, std::vector<Color>& image, int threads = 0,
                      int max_depth = DEFAULT_MAX_DEPTH);

// Returns `threads`, or the number of hardware threads if it is 0.
int resolve_threads(int threads);

// Settings shared by every renderer backend.
struct RenderSettings {
    int threads = 0;                    // 0 = one per hardware thread
    int max_depth = DEFAULT_MAX_DEPTH;  // reflection/refraction bounces
    float resolution_scale = 1.0f;      // fraction of the window resolution to render
};

// A rendering backend. The app picks one at runtime; the CPU renderer is always
// available, so it is the fallback when a GPU backend isn't.
class Renderer {
public:
    virtual ~Renderer() = default;
    virtual const char* name() const = 0;
    virtual bool available() const { return true; }

    // Renders one frame into `image` (row-major, bottom row first).
    // Returns the number of rays traced.
    virtual uint64_t render(const Camera& camera, const Scene& scene, const RenderSettings& settings,
                            int width, int height, std::vector<Color>& image) = 0;
};

// Multithreaded CPU Whitted-style raytracer.
class CpuRenderer : public Renderer {
public:
    const char* name() const override { return "CPU"; }
    uint64_t render(const Camera& camera, const Scene& scene, const RenderSettings& settings,
                    int width, int height, std::vector<Color>& image) override;
};
