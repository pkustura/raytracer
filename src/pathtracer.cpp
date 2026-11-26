#include "pathtracer.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <thread>

using namespace std;

namespace {

const float EPS = 1e-3f;                     // offset for secondary ray origins
const float SUN_ANGULAR_RADIUS_DEG = 3.0f;   // apparent size of the sun (soft shadow width)
const float SKY_SCALE = 0.6f;                // sky brightness relative to the background gradient
const int RUSSIAN_ROULETTE_DEPTH = 3;        // start terminating dim paths after this many bounces
const float INV_PI = 1.0f / float(M_PI);

// ---------------------------------------------------------------- helpers

// PCG32 random number generator (pcg-random.org), seeded per pixel and sample.
struct Rng {
    uint64_t state;

    explicit Rng(uint64_t seed) : state(0) {
        next();
        state += seed;
        next();
    }
    uint32_t next() {
        uint64_t old = state;
        state = old * 6364136223846793005ULL + 1442695040888963407ULL;
        uint32_t xorshifted = uint32_t(((old >> 18u) ^ old) >> 27u);
        uint32_t rot = uint32_t(old >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((32u - rot) & 31u));
    }
    float uniform() { return (next() >> 8) * (1.0f / 16777216.0f); }  // [0, 1)
};

uint64_t hash64(uint64_t x) {  // splitmix64 finalizer
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

Vector3 mul(const Vector3& a, const Vector3& b) { return Vector3(a.x * b.x, a.y * b.y, a.z * b.z); }
Vector3 to_vec(const Color& c) { return Vector3(c.r, c.g, c.b); }
float max_component(const Vector3& v) { return max(v.x, max(v.y, v.z)); }
float luminance(const Vector3& v) { return 0.2126f * v.x + 0.7152f * v.y + 0.0722f * v.z; }

// Builds an orthonormal basis (t, b, n) around unit vector n (Duff et al. 2017).
void basis(const Vector3& n, Vector3& t, Vector3& b) {
    float sign = copysign(1.0f, n.z);
    float a = -1.0f / (sign + n.z);
    float bb = n.x * n.y * a;
    t = Vector3(1.0f + sign * n.x * n.x * a, sign * bb, -sign * n.x);
    b = Vector3(bb, sign + n.y * n.y * a, -n.y);
}

// Direction around `axis` with the given cosine from it and azimuth phi.
Vector3 around(const Vector3& axis, float cos_theta, float phi) {
    Vector3 t, b;
    basis(axis, t, b);
    float sin_theta = sqrt(max(0.0f, 1.0f - cos_theta * cos_theta));
    return t * (sin_theta * cos(phi)) + b * (sin_theta * sin(phi)) + axis * cos_theta;
}

// Cosine-weighted direction in the hemisphere around n (pdf = cos / pi).
Vector3 sample_cosine(const Vector3& n, Rng& rng) {
    float u1 = rng.uniform(), u2 = rng.uniform();
    return around(n, sqrt(1.0f - u1), 2.0f * float(M_PI) * u2);
}

// Direction in the Phong lobe around r (pdf = (e + 1) / (2 pi) * cos^e(alpha)).
Vector3 sample_phong(const Vector3& r, float exponent, Rng& rng) {
    float u1 = rng.uniform(), u2 = rng.uniform();
    return around(r, pow(u1, 1.0f / (exponent + 1.0f)), 2.0f * float(M_PI) * u2);
}

// Uniform direction within the cone around `axis` (pdf = 1 / solid angle).
Vector3 sample_cone(const Vector3& axis, float cos_max, Rng& rng) {
    float u1 = rng.uniform(), u2 = rng.uniform();
    return around(axis, 1.0f - u1 * (1.0f - cos_max), 2.0f * float(M_PI) * u2);
}

// --------------------------------------------------------- scene lighting

struct Lighting {
    Vector3 sun_dir;        // unit vector toward the sun
    float sun_cos_max;      // cosine of the sun's angular radius
    Vector3 sun_irradiance; // E: irradiance on a surface facing the sun
    Vector3 sun_radiance;   // L = E / solid angle, seen when looking at the sun disc
};

Lighting make_lighting(const Scene& scene) {
    Lighting l;
    l.sun_dir = (scene.light.position - scene.camera_at).normalized();
    l.sun_cos_max = cos(SUN_ANGULAR_RADIUS_DEG * float(M_PI) / 180.0f);
    // Lambert BRDF is albedo/pi, so E = pi * I matches the Whitted renderer's
    // direct diffuse term (albedo * cos * I).
    l.sun_irradiance = scene.light.intensity * float(M_PI);
    float solid_angle = 2.0f * float(M_PI) * (1.0f - l.sun_cos_max);
    l.sun_radiance = l.sun_irradiance / solid_angle;
    return l;
}

Vector3 sky(const Vector3& dir) {
    float t = 0.5f * (dir.y + 1.0f);
    return (Vector3(1, 1, 1) * (1.0f - t) + Vector3(0.5f, 0.7f, 1.0f) * t) * SKY_SCALE;
}

// ------------------------------------------------------------- tracing

bool closest_hit(const Ray& ray, const vector<Shape*>& shapes, float& t, Vector3& normal, const Shape*& hit) {
    t = numeric_limits<float>::max();
    hit = nullptr;
    for (const Shape* shape : shapes) {
        float t_temp;
        Vector3 n_temp;
        if (shape->intersect(ray, t_temp, n_temp) && t_temp > EPS && t_temp < t) {
            t = t_temp;
            normal = n_temp;
            hit = shape;
        }
    }
    return hit != nullptr;
}

bool occluded(const Ray& ray, const vector<Shape*>& shapes) {
    for (const Shape* shape : shapes) {
        float t;
        Vector3 n;
        if (shape->intersect(ray, t, n) && t > EPS) return true;
    }
    return false;
}

float fresnel_schlick(float cosi, float etai, float etat) {
    float r0 = (etai - etat) / (etai + etat);
    r0 = r0 * r0;
    return r0 + (1.0f - r0) * pow(1.0f - cosi, 5.0f);
}

// Traces one path from the camera and returns the radiance it carries.
// Caps the brightest channel of an indirect contribution at `limit` (0 = no cap).
Vector3 clamp_contribution(const Vector3& c, float limit) {
    float m = max_component(c);
    return (limit > 0.0f && m > limit) ? c * (limit / m) : c;
}

Vector3 trace_path(Ray ray, const vector<Shape*>& shapes, const Lighting& light, int max_depth,
                   bool caustics, float firefly_clamp, Rng& rng, uint64_t& rays) {
    Vector3 radiance(0, 0, 0);
    Vector3 throughput(1, 1, 1);
    bool specular_bounce = true;  // camera rays and mirror/glass bounces can see the sun disc
    bool diffuse_seen = false;    // has the path bounced off a diffuse/glossy surface yet?

    for (int depth = 0;; ++depth) {
        ++rays;
        float t;
        Vector3 normal;
        const Shape* hit;
        if (!closest_hit(ray, shapes, t, normal, hit)) {
            Vector3 env = sky(ray.direction);
            // The sun is sampled directly at diffuse/glossy bounces, so only add
            // its disc here when it couldn't have been (avoids counting it twice)
            // A diffuse -> mirror/glass -> sun path is a caustic; skip it if caustics are off
            bool sun_visible = specular_bounce && (caustics || !diffuse_seen);
            if (sun_visible && ray.direction.dot(light.sun_dir) >= light.sun_cos_max) {
                env += light.sun_radiance;
            }
            // Light reached after a diffuse/glossy bounce is indirect: clamp it so
            // rare, very bright paths (caustics) don't show up as fireflies
            Vector3 c = mul(throughput, env);
            radiance += diffuse_seen ? clamp_contribution(c, firefly_clamp) : c;
            break;
        }

        const Material& m = hit->material;
        Vector3 p = ray.point_at_parameter(t);
        Vector3 dir = ray.direction;
        bool front_face = dir.dot(normal) < 0;
        Vector3 n = front_face ? normal : -1 * normal;  // faces the incoming ray

        if (m.transparency > 0.0f) {
            // Glass: pick reflection or refraction by Fresnel reflectance
            if (depth >= max_depth) break;
            float etai = front_face ? 1.0f : m.ior;
            float etat = front_face ? m.ior : 1.0f;
            Vector3 refracted;
            float kr = 1.0f;  // total internal reflection unless refraction succeeds
            if (refract(dir, normal, m.ior, refracted)) {
                refracted = refracted.normalized();
                float cos_i = min(-dir.dot(n), 1.0f);
                float cos_f = etai > etat ? min(-refracted.dot(n), 1.0f) : cos_i;  // less dense side
                kr = fresnel_schlick(cos_f, etai, etat);
            }
            if (rng.uniform() < kr) {
                ray = Ray(p + n * EPS, reflect(dir, n).normalized());
            } else {
                ray = Ray(p - n * EPS, refracted);
                throughput = throughput * m.transparency;
            }
            specular_bounce = true;
        } else if (max_component(to_vec(m.mirror_reflectance)) > 0.0f) {
            // Mirror: perfect reflection
            if (depth >= max_depth) break;
            throughput = mul(throughput, to_vec(m.mirror_reflectance));
            ray = Ray(p + n * EPS, reflect(dir, n).normalized());
            specular_bounce = true;
        } else {
            // Diffuse + Phong glossy
            Vector3 kd = to_vec(m.diffuse);
            Vector3 ks = to_vec(m.specular);
            // Scale specular uniformly so kd + ks <= 1 in every channel (energy conservation)
            float scale = 1.0f;
            if (ks.x > 0) scale = min(scale, max(0.0f, 1.0f - kd.x) / ks.x);
            if (ks.y > 0) scale = min(scale, max(0.0f, 1.0f - kd.y) / ks.y);
            if (ks.z > 0) scale = min(scale, max(0.0f, 1.0f - kd.z) / ks.z);
            ks = ks * scale;
            float exponent = max(m.shininess, 1.0f);
            Vector3 mirror_dir = reflect(dir, n);

            // Direct light from the sun (next event estimation)
            Vector3 wi = sample_cone(light.sun_dir, light.sun_cos_max, rng);
            float cos_l = n.dot(wi);
            if (cos_l > 0.0f) {
                ++rays;
                if (!occluded(Ray(p + n * EPS, wi), shapes)) {
                    float cos_alpha = max(0.0f, wi.dot(mirror_dir));
                    Vector3 brdf = kd * INV_PI + ks * ((exponent + 2.0f) * 0.5f * INV_PI * pow(cos_alpha, exponent));
                    Vector3 c = mul(throughput, mul(brdf, light.sun_irradiance)) * cos_l;
                    radiance += diffuse_seen ? clamp_contribution(c, firefly_clamp) : c;
                }
            }
            if (depth >= max_depth) break;

            // Continue the path: pick the diffuse or glossy lobe by luminance
            float pd = luminance(kd), ps = luminance(ks);
            if (pd + ps <= 0.0f) break;
            float prob_diffuse = pd / (pd + ps);
            Vector3 next;
            if (rng.uniform() < prob_diffuse) {
                next = sample_cosine(n, rng);
                throughput = mul(throughput, kd) / prob_diffuse;
            } else {
                next = sample_phong(mirror_dir.normalized(), exponent, rng);
                float cos_n = next.dot(n);
                if (cos_n <= 0.0f) break;  // sampled below the surface
                throughput = mul(throughput, ks) * ((exponent + 2.0f) / (exponent + 1.0f) * cos_n / (1.0f - prob_diffuse));
            }
            ray = Ray(p + n * EPS, next);
            specular_bounce = false;
            diffuse_seen = true;
        }

        // Russian roulette: randomly end dim paths, boosting the survivors to stay unbiased
        if (depth >= RUSSIAN_ROULETTE_DEPTH) {
            float survive = min(0.95f, max_component(throughput));
            if (rng.uniform() >= survive) break;
            throughput = throughput / survive;
        }
    }
    return radiance;
}

}  // namespace

bool PathTracer::needs_reset(const Camera& camera, const Scene& scene, const RenderSettings& settings,
                             int width, int height) const {
    auto same = [](const Vector3& a, const Vector3& b) { return a.x == b.x && a.y == b.y && a.z == b.z; };
    return samples_ == 0 || width != width_ || height != height_ || settings.max_depth != max_depth_ ||
           settings.caustics != caustics_ || settings.firefly_clamp != firefly_clamp_ ||
           &scene != scene_ || !same(camera.origin, cam_origin_) ||
           !same(camera.lower_left_corner, cam_corner_) || !same(camera.horizontal, cam_horizontal_) ||
           !same(camera.vertical, cam_vertical_);
}

uint64_t PathTracer::render(const Camera& camera, const Scene& scene, const RenderSettings& settings,
                            int width, int height, vector<Color>& image) {
    if (needs_reset(camera, scene, settings, width, height)) {
        accum_.assign(size_t(width) * height, Accum{0, 0, 0});
        samples_ = 0;
        width_ = width;
        height_ = height;
        max_depth_ = settings.max_depth;
        caustics_ = settings.caustics;
        firefly_clamp_ = settings.firefly_clamp;
        scene_ = &scene;
        cam_origin_ = camera.origin;
        cam_corner_ = camera.lower_left_corner;
        cam_horizontal_ = camera.horizontal;
        cam_vertical_ = camera.vertical;
    }

    int spp = max(1, settings.samples_per_frame);
    if (settings.max_samples > 0) spp = min(spp, settings.max_samples - samples_);

    uint64_t total_rays = 0;
    if (spp > 0) {
        const Lighting lighting = make_lighting(scene);
        const int first_sample = samples_;
        const int ROWS_PER_BATCH = 4;
        atomic<int> next_row{0};
        atomic<uint64_t> rays_counter{0};

        auto worker = [&]() {
            uint64_t rays = 0;
            for (;;) {
                int row0 = next_row.fetch_add(ROWS_PER_BATCH);
                if (row0 >= height) break;
                int row1 = min(row0 + ROWS_PER_BATCH, height);
                for (int j = row0; j < row1; ++j) {
                    for (int i = 0; i < width; ++i) {
                        size_t pixel = size_t(j) * width + i;
                        Vector3 sum(0, 0, 0);
                        for (int s = 0; s < spp; ++s) {
                            Rng rng(hash64(pixel * 0x100000001b3ULL + uint64_t(first_sample + s)));
                            // Jitter within the pixel for anti-aliasing
                            float u = (i + rng.uniform()) / float(width);
                            float v = (j + rng.uniform()) / float(height);
                            sum += trace_path(camera.get_ray(u, v), scene.shapes, lighting,
                                              settings.max_depth, settings.caustics,
                                              settings.firefly_clamp, rng, rays);
                        }
                        Accum& a = accum_[pixel];
                        a.r += sum.x;
                        a.g += sum.y;
                        a.b += sum.z;
                    }
                }
            }
            rays_counter += rays;
        };

        int threads = resolve_threads(settings.threads);
        vector<thread> pool;
        pool.reserve(threads - 1);
        for (int t = 1; t < threads; ++t) pool.emplace_back(worker);
        worker();
        for (auto& th : pool) th.join();

        samples_ += spp;
        total_rays = rays_counter;
    }

    // Resolve the running average into the displayed image
    image.resize(size_t(width) * height);
    float inv = samples_ > 0 ? 1.0f / samples_ : 0.0f;
    for (size_t i = 0; i < image.size(); ++i) {
        image[i] = Color(accum_[i].r * inv, accum_[i].g * inv, accum_[i].b * inv).clamped();
    }
    return total_rays;
}
