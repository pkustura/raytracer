// Headless benchmark: renders the default scene with the camera orbiting the
// models and reports frame rate and ray throughput.
//
// Usage: Benchmark [--frames N] [--width W] [--height H] [--threads T] [--depth D]
//                  [--bvh sah|median] [--renderer whitted|path] [--spp S] [--out frame.ppm]
//        --threads 0 (the default) uses one thread per hardware thread.
//
// The same benchmark can be run from the interactive app's settings panel.

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "benchmark.h"
#include "camera.h"
#include "pathtracer.h"
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
    int depth = DEFAULT_MAX_DEPTH;
    bool path = false;
    int spp = 1;
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
        else if (arg == "--depth") depth = atoi(next());
        else if (arg == "--spp") spp = atoi(next());
        else if (arg == "--renderer") {
            string v = next();
            if (v == "whitted") path = false;
            else if (v == "path") path = true;
            else { cerr << "--renderer must be 'whitted' or 'path'" << endl; return 1; }
        }
        else if (arg == "--bvh") {
            string v = next();
            if (v == "sah") bvh = BvhBuilder::SAH;
            else if (v == "median") bvh = BvhBuilder::Median;
            else { cerr << "--bvh must be 'sah' or 'median'" << endl; return 1; }
        }
        else if (arg == "--out") out = next();
        else { cerr << "usage: " << argv[0] << " [--frames N] [--width W] [--height H] [--threads T] [--depth D] [--bvh sah|median] [--renderer whitted|path] [--spp S] [--out frame.ppm]" << endl; return 1; }
    }

    auto t_load = Clock::now();
    Scene scene;
    if (!build_default_scene(scene, bvh)) return 1;
    double load_s = chrono::duration<double>(Clock::now() - t_load).count();

    CpuRenderer whitted;
    PathTracer path_tracer;
    Renderer& renderer = path ? static_cast<Renderer&>(path_tracer) : static_cast<Renderer&>(whitted);
    RenderSettings settings;
    settings.threads = threads;
    settings.max_depth = depth;
    settings.samples_per_frame = spp;
    settings.max_samples = 0;

    BenchmarkRun run(scene, frames, width, height, resolve_threads(threads), depth);
    if (path) run.set_samples_per_frame(spp);
    vector<Color> image;
    while (!run.done()) {
        Camera camera = run.camera();
        auto t0 = Clock::now();
        uint64_t rays = renderer.render(camera, scene, settings, width, height, image);
        double frame_s = chrono::duration<double>(Clock::now() - t0).count();
        if (run.frame() == 0 && !out.empty()) write_ppm(out, image, width, height);
        run.record(frame_s, rays);
    }

    const BenchmarkResult& r = run.result();
    printf("Scene load + BVH build: %.3f s\n", load_s);
    printf("Renderer: %s%s\n", renderer.name(), path ? (", " + to_string(spp) + " spp/frame").c_str() : "");
    printf("Frames: %d at %dx%d, %d threads, %s BVH, depth %d\n", r.frames, r.width, r.height, r.threads,
           bvh == BvhBuilder::SAH ? "SAH" : "median-split", r.max_depth);
    printf("Average: %.2f fps (%.1f ms/frame, min %.1f, max %.1f)\n",
           r.fps(), r.ms_per_frame(), 1000.0 * r.min_frame, 1000.0 * r.max_frame);
    printf("Primary rays: %.2f Mrays/s\n", r.primary_mrays_per_sec());
    printf("All rays:     %.2f Mrays/s (%.2f rays/pixel)\n", r.all_mrays_per_sec(), r.rays_per_pixel());
    return 0;
}
