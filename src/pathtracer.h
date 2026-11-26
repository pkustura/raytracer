#pragma once
#include <cstdint>
#include <vector>
#include "renderer.h"

// Progressive CPU path tracer.
//
// Lighting comes from two sources:
//  - the sky: the background gradient, which lights the scene from every direction
//  - the sun: a small disc in the direction of the scene's light, sampled
//    directly with shadow rays at every bounce (soft shadows, low noise)
//
// Materials are interpreted as:
//  - diffuse / Phong: Lambertian diffuse plus a normalized Phong glossy lobe
//    (specular is scaled down uniformly if diffuse + specular would exceed 1)
//  - mirror: perfect reflection tinted by mirror_reflectance
//  - glass: Fresnel-weighted choice between reflection and refraction;
//    transparency tints the refracted light
// The material's ambient term is ignored; the sky replaces it.
//
// Light that arrives after a diffuse/glossy bounce can be clamped (firefly_clamp)
// so rare, very bright caustic paths don't show up as speckles. This trades a
// dimmer caustic for a much cleaner image; direct light is never clamped.
//
// While the camera, resolution and settings stay the same, each frame adds
// samples_per_frame samples per pixel to a running average, up to max_samples.
class PathTracer : public Renderer {
public:
    const char* name() const override { return "CPU path tracer"; }
    bool progressive() const override { return true; }
    int sample_count() const override { return samples_; }
    void reset() override { samples_ = 0; }

    uint64_t render(const Camera& camera, const Scene& scene, const RenderSettings& settings,
                    int width, int height, std::vector<Color>& image) override;

private:
    struct Accum { float r, g, b; };

    std::vector<Accum> accum_;
    int samples_ = 0;

    // State of the last frame, to detect when accumulation must restart
    int width_ = 0, height_ = 0, max_depth_ = 0;
    bool caustics_ = true;
    float firefly_clamp_ = 20.0f;
    const Scene* scene_ = nullptr;
    Vector3 cam_origin_, cam_corner_, cam_horizontal_, cam_vertical_;

    bool needs_reset(const Camera& camera, const Scene& scene, const RenderSettings& settings,
                     int width, int height) const;
};
