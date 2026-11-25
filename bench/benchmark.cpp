// Headless benchmark: renders the default scene with the camera orbiting the
// models and reports frame rate and ray throughput.
//
// Usage: Benchmark [--frames N] [--width W] [--height H] [--threads T] [--bvh sah|median] [--out frame.ppm]
//        --threads 0 (the default) uses one thread per hardware thread.

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include "camera.h"
#include "renderer.h"
#include "scene.h"

using namespace std;
using Clock = chrono::steady_clock;

static void write_ppm(const string& filename, const vector<Color>& image, int width, int height) {
    ofstream file(filename);
    file << "P3\n" << width << " " << height << "\n255\n";
    // image rows are stored bottom-first; PPM wants top-first
    for (int j = height - 1; j >= 0; --j) {
        for (int i = 0; i < width; ++i) {
            const Color& c = image[j * width + i];
            file << int(255.99 * c.r) << " " << int(255.99 * c.g) << " " << int(255.99 * c.b) << "\n";
        }
    }
}

int main(int argc, char** argv) {
    int frames = 60;
    int width = 800;
    int height = 600;
    int threads = 0;
    BvhBuilder bvh = BvhBuilder::SAH;
    string out;

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        auto next = [&]() -> const char* {
            if (i + 1 >= argc) { cerr << "missing value for " << arg << endl; exit(1); }
            return argv[++i];
        };
        if (arg == "--frames") frames = atoi(next());
        else if (arg == "--width") width = atoi(next());
        else if (arg == "--height") height = atoi(next());
        else if (arg == "--threads") threads = atoi(next());
        else if (arg == "--bvh") {
            string v = next();
            if (v == "sah") bvh = BvhBuilder::SAH;
            else if (v == "median") bvh = BvhBuilder::Median;
            else { cerr << "--bvh must be 'sah' or 'median'" << endl; return 1; }
        }
        else if (arg == "--out") out = next();
        else { cerr << "usage: " << argv[0] << " [--frames N] [--width W] [--height H] [--threads T] [--bvh sah|median] [--out frame.ppm]" << endl; return 1; }
    }

    auto t_load = Clock::now();
    Scene scene;
    if (!build_default_scene(scene, bvh)) return 1;
    double load_s = chrono::duration<double>(Clock::now() - t_load).count();

    // Orbit the camera once around the scene's look-at point over the run
    Vector3 target = scene.camera_at;
    Vector3 offset = scene.camera_from - target;
    float radius = sqrt(offset.x * offset.x + offset.z * offset.z);
    float height_above = offset.y;
    float start_angle = atan2(offset.z, offset.x);
    float aspect = float(width) / float(height);

    vector<Color> image;
    uint64_t total_rays = 0;
    double total_s = 0.0;
    double min_frame = 1e30, max_frame = 0.0;

    for (int f = 0; f < frames; ++f) {
        float angle = start_angle + 2.0f * float(M_PI) * f / frames;
        Vector3 from = target + Vector3(radius * cos(angle), height_above, radius * sin(angle));
        Camera camera(from, target, Vector3(0, 1, 0), scene.vfov, aspect, 0.1f);

        auto t0 = Clock::now();
        total_rays += render_frame(camera, scene.shapes, scene.light, width, height, image, threads);
        double frame_s = chrono::duration<double>(Clock::now() - t0).count();

        total_s += frame_s;
        min_frame = min(min_frame, frame_s);
        max_frame = max(max_frame, frame_s);

        if (f == 0 && !out.empty()) write_ppm(out, image, width, height);
    }

    uint64_t primary = uint64_t(width) * height * frames;
    printf("Scene load + BVH build: %.3f s\n", load_s);
    printf("Frames: %d at %dx%d, %d threads, %s BVH\n", frames, width, height,
           threads > 0 ? threads : int(std::thread::hardware_concurrency()),
           bvh == BvhBuilder::SAH ? "SAH" : "median-split");
    printf("Average: %.2f fps (%.1f ms/frame, min %.1f, max %.1f)\n",
           frames / total_s, 1000.0 * total_s / frames, 1000.0 * min_frame, 1000.0 * max_frame);
    printf("Primary rays: %.2f Mrays/s\n", primary / total_s / 1e6);
    printf("All rays:     %.2f Mrays/s (%.2f rays/pixel)\n", total_rays / total_s / 1e6, double(total_rays) / primary);
    return 0;
}
