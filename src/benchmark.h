#pragma once
#include <cstdint>
#include "camera.h"
#include "scene.h"

// Results of a benchmark run. Timings cover rendering only, not display.
struct BenchmarkResult {
    int frames = 0;
    int width = 0, height = 0;
    int threads = 0;
    int max_depth = 0;
    double total_seconds = 0.0;
    double min_frame = 0.0, max_frame = 0.0;
    uint64_t rays = 0;

    double fps() const { return total_seconds > 0 ? frames / total_seconds : 0.0; }
    double ms_per_frame() const { return frames > 0 ? 1000.0 * total_seconds / frames : 0.0; }
    uint64_t primary_rays() const { return uint64_t(width) * height * frames; }
    double primary_mrays_per_sec() const { return total_seconds > 0 ? primary_rays() / total_seconds / 1e6 : 0.0; }
    double all_mrays_per_sec() const { return total_seconds > 0 ? rays / total_seconds / 1e6 : 0.0; }
    double rays_per_pixel() const { return primary_rays() > 0 ? double(rays) / primary_rays() : 0.0; }
};

// The standard benchmark: one full camera orbit around the scene's look-at
// point over `frames` frames. Drive it one frame at a time:
//
//   BenchmarkRun run(scene, 60, aspect);
//   while (!run.done()) { Camera cam = run.camera(); ...render...; run.record(seconds, rays); }
//
// Used by both the headless Benchmark tool and the in-app benchmark mode.
class BenchmarkRun {
public:
    BenchmarkRun(const Scene& scene, int frames, int width, int height, int threads, int max_depth);

    bool done() const { return frame_ >= result_.frames; }
    int frame() const { return frame_; }
    int frames() const { return result_.frames; }
    float progress() const { return result_.frames > 0 ? float(frame_) / result_.frames : 1.0f; }

    // Camera for the current frame
    Camera camera() const;

    // Records the current frame's render time and ray count, then advances.
    void record(double seconds, uint64_t rays);

    const BenchmarkResult& result() const { return result_; }

private:
    Vector3 target_;
    float radius_, height_, start_angle_, vfov_;
    int frame_ = 0;
    BenchmarkResult result_;
};
