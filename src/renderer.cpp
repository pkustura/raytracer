#include "renderer.h"
#include <algorithm>
#include <atomic>
#include <thread>
#include <cmath>
#include <limits>

using namespace std;

namespace {
// Counts every ray traced (primary + secondary) on the calling thread.
thread_local uint64_t rays_traced = 0;
}

Vector3 reflect(const Vector3& I, const Vector3& N) {
    return I - 2.0f * N.dot(I) * N;
}

bool refract(const Vector3& I, const Vector3& N, float ior, Vector3& refracted) {
    float cosi = clamp(I.dot(N), -1.0f, 1.0f);
    float etai = 1, etat = ior;
    Vector3 n = N;
    if (cosi < 0) {
        cosi = -cosi;
    } else {
        swap(etai, etat);
        n = -1*N;
    }
    float eta = etai / etat;
    float k = 1 - eta * eta * (1 - cosi * cosi);
    if (k < 0) {
        return false;
    } else {
        refracted = eta * I + (eta * cosi - sqrt(k)) * n;
        return true;
    }
}


Color shade(const Material& material, const Vector3& normal, const Vector3& viewDir, const Vector3& lightDir, const Vector3& lightColor) {
    // ambient
    Color ambient = material.ambient * lightColor;

    // diffuse
    float diff = max(normal.dot(lightDir), 0.0f);
    Color diffuse = material.diffuse * diff * lightColor;

    // specular
    Vector3 reflectDir = reflect(-1*lightDir, normal);
    float spec = pow(max(viewDir.dot(reflectDir), 0.0f), material.shininess);
    Color specular = material.specular * spec * lightColor;

    // combine results (reflection/transparency are handled in ray_color)
    return ambient + diffuse + specular;
}


// Schlick's approximation of the Fresnel reflectance
float fresnel_schlick(float cosi, float etai, float etat) {
    float r0 = (etai - etat) / (etai + etat);
    r0 = r0 * r0;
    return r0 + (1.0f - r0) * pow(1.0f - cosi, 5.0f);
}

const float RAY_EPSILON = 1e-3f;

// Function to calculate the color of a ray by checking for intersections with shapes
Color ray_color(const Ray& ray, const vector<Shape*>& shapes, const Light& light, int depth, int max_depth) {
    ++rays_traced;
    float t_min = RAY_EPSILON;
    float t = numeric_limits<float>::max();
    Shape* hit_shape = nullptr;
    Vector3 normal;

    for (const auto& shape : shapes) {
        float t_temp;
        Vector3 n_temp;
        if (shape->intersect(ray, t_temp, n_temp) && t_temp > t_min && t_temp < t) {
            t = t_temp;
            normal = n_temp;
            hit_shape = shape;
        }
    }

    if (!hit_shape) {
        // Background color
        Vector3 unit_direction = ray.direction.normalized();
        float t_background = 0.5 * (unit_direction.y + 1.0);
        return (Color(1.0, 1.0, 1.0) * (1.0 - t_background) + Color(0.5, 0.7, 1.0) * t_background).clamped();
    }

    const Material& material = hit_shape->material;

    Vector3 hit_point = ray.point_at_parameter(t);
    Vector3 dir = ray.direction.normalized();

    // Normal facing against the incoming ray (flips when we're inside an object)
    bool front_face = dir.dot(normal) < 0;
    Vector3 facing_normal = front_face ? normal : -1 * normal;

    Vector3 light_dir = (light.get_position() - hit_point).normalized();
    Vector3 view_dir = -1 * dir;

    // Local (direct) lighting
    Color local = shade(material, facing_normal, view_dir, light_dir, light.intensity);

    if (depth >= max_depth) {
        return local.clamped();
    }

    Vector3 reflect_dir = reflect(dir, facing_normal).normalized();
    Ray reflect_ray(hit_point + facing_normal * RAY_EPSILON, reflect_dir);

    // Glass: blend reflection and refraction using Fresnel
    if (material.transparency > 0.0f) {
        float etai = front_face ? 1.0f : material.ior;
        float etat = front_face ? material.ior : 1.0f;
        float cosi = min(view_dir.dot(facing_normal), 1.0f);

        Vector3 refract_dir;
        float kr;
        Color refracted;
        if (refract(dir, normal, material.ior, refract_dir)) {
            // Schlick must use the cosine on the less dense side
            float cos_fresnel = cosi;
            if (etai > etat) {
                cos_fresnel = min(-refract_dir.normalized().dot(facing_normal), 1.0f);
            }
            kr = fresnel_schlick(cos_fresnel, etai, etat);
            Ray refract_ray(hit_point - facing_normal * RAY_EPSILON, refract_dir.normalized());
            refracted = ray_color(refract_ray, shapes, light, depth + 1, max_depth);
        } else {
            kr = 1.0f; // total internal reflection
        }

        Color reflected = ray_color(reflect_ray, shapes, light, depth + 1, max_depth);
        Color transmitted = reflected * kr + refracted * (1.0f - kr);

        return (local * (1.0f - material.transparency) + transmitted * material.transparency).clamped();
    }

    // Mirror: add tinted reflection on top of local lighting
    const Color& mr = material.mirror_reflectance;
    if (mr.r > 0.0f || mr.g > 0.0f || mr.b > 0.0f) {
        Color reflected = ray_color(reflect_ray, shapes, light, depth + 1, max_depth);
        return (local + reflected * mr).clamped();
    }

    return local.clamped();
}

uint64_t render_frame(const Camera& camera, const vector<Shape*>& shapes, const Light& light,
                      int width, int height, vector<Color>& image, int threads, int max_depth) {
    image.resize(static_cast<size_t>(width) * height);
    if (threads <= 0) threads = max(1u, thread::hardware_concurrency());

    // Workers grab small batches of rows from a shared counter, so threads that
    // land on cheap rows (sky, floor) pick up more work than ones on glass.
    const int ROWS_PER_BATCH = 4;
    atomic<int> next_row{0};
    atomic<uint64_t> total_rays{0};

    auto worker = [&]() {
        uint64_t before = rays_traced;
        for (;;) {
            int row0 = next_row.fetch_add(ROWS_PER_BATCH);
            if (row0 >= height) break;
            int row1 = min(row0 + ROWS_PER_BATCH, height);
            for (int j = row0; j < row1; ++j) {
                for (int i = 0; i < width; ++i) {
                    float u = float(i) / float(width);
                    float v = float(j) / float(height);
                    image[j * width + i] = ray_color(camera.get_ray(u, v), shapes, light, 0, max_depth);
                }
            }
        }
        total_rays += rays_traced - before;
    };

    vector<thread> pool;
    pool.reserve(threads - 1);
    for (int t = 1; t < threads; ++t) pool.emplace_back(worker);
    worker();  // the calling thread works too
    for (auto& th : pool) th.join();

    return total_rays;
}

// ------------------------------------------------------------- CpuRenderer

int resolve_threads(int threads) {
    return threads > 0 ? threads : static_cast<int>(max(1u, thread::hardware_concurrency()));
}

uint64_t CpuRenderer::render(const Camera& camera, const Scene& scene, const RenderSettings& settings,
                             int width, int height, vector<Color>& image) {
    return render_frame(camera, scene.shapes, scene.light, width, height, image,
                        resolve_threads(settings.threads), settings.max_depth);
}
