#pragma once
#include <memory>
#include <string>
#include <vector>
#include "benchmark.h"
#include "camera.h"
#include "mesh.h"
#include "renderer.h"
#include "scene.h"

// Everything the interactive app shows or lets the user change.
struct AppState {
    // Scene
    std::unique_ptr<Scene> scene;
    BvhBuilder bvh = BvhBuilder::SAH;
    bool scene_ok = false;
    double scene_load_seconds = 0.0;
    size_t triangle_count = 0;

    // Camera the user flies around
    std::unique_ptr<Camera> camera;

    // Rendering
    std::vector<std::unique_ptr<Renderer>> renderers;
    int renderer_index = 0;
    RenderSettings settings;
    int window_width = 800, window_height = 600;  // render resolution at scale 1.0

    // Input
    bool ui_visible = false;      // settings panel open (cursor free, no mouse look)
    bool reset_mouse = true;      // ignore the next mouse delta (after capture changes)

    // Live stats (smoothed)
    double render_ms = 0.0;
    double frame_ms = 0.0;
    double mrays_per_sec = 0.0;
    int render_width = 0, render_height = 0;

    // Benchmark mode
    int bench_frames = 60;
    std::unique_ptr<BenchmarkRun> bench;   // non-null while a benchmark is running
    bool has_bench_result = false;
    BenchmarkResult last_bench;
    std::string last_bench_renderer;
    std::string last_bench_bvh;

    // Requests from the UI, handled by the main loop
    bool request_scene_rebuild = false;
    bool request_camera_reset = false;
    bool request_bench_start = false;
    bool request_bench_cancel = false;

    Renderer& renderer() { return *renderers[renderer_index]; }
    bool benchmarking() const { return bench != nullptr; }
};

// Draws the settings panel (when ui_visible) or a small hint overlay.
void draw_ui(AppState& app);

const char* bvh_name(BvhBuilder bvh);
