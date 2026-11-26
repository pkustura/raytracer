#include "app_ui.h"
#include <thread>
#include "imgui.h"

const char* bvh_name(BvhBuilder bvh) {
    return bvh == BvhBuilder::SAH ? "SAH" : "Median split";
}

namespace {

void draw_hint() {
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                   ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs;
    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::SetNextWindowBgAlpha(0.45f);
    if (ImGui::Begin("##hint", nullptr, flags)) {
        ImGui::TextUnformatted("Tab: settings   WASD + mouse: move   Shift: faster   Esc: quit");
    }
    ImGui::End();
}

void draw_stats(AppState& app) {
    if (!ImGui::CollapsingHeader("Stats", ImGuiTreeNodeFlags_DefaultOpen)) return;
    double fps = app.frame_ms > 0 ? 1000.0 / app.frame_ms : 0.0;
    ImGui::Text("FPS:          %.1f  (%.1f ms/frame)", fps, app.frame_ms);
    ImGui::Text("Render:       %.1f ms", app.render_ms);
    ImGui::Text("Throughput:   %.2f Mrays/s", app.mrays_per_sec);
    ImGui::Text("Resolution:   %d x %d", app.render_width, app.render_height);
    ImGui::Text("Triangles:    %zu", app.triangle_count);
    ImGui::Text("Scene build:  %.2f s (%s BVH)", app.scene_load_seconds, bvh_name(app.bvh));
    if (!app.scene_ok) {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Some models failed to load (run ./fetch_models.sh)");
    }
}

void draw_settings(AppState& app) {
    if (!ImGui::CollapsingHeader("Graphics", ImGuiTreeNodeFlags_DefaultOpen)) return;
    ImGui::BeginDisabled(app.benchmarking());

    // Renderer backend; unavailable backends are listed but can't be picked
    if (ImGui::BeginCombo("Renderer", app.renderer().name())) {
        for (int i = 0; i < int(app.renderers.size()); ++i) {
            Renderer& r = *app.renderers[i];
            ImGui::BeginDisabled(!r.available());
            if (ImGui::Selectable(r.name(), i == app.renderer_index)) app.renderer_index = i;
            ImGui::EndDisabled();
        }
        ImGui::EndCombo();
    }

    // Threads (0 = auto)
    int hw = std::max(1u, std::thread::hardware_concurrency());
    bool auto_threads = app.settings.threads == 0;
    if (ImGui::Checkbox("Auto threads", &auto_threads)) app.settings.threads = auto_threads ? 0 : hw;
    if (!auto_threads) {
        ImGui::SliderInt("Threads", &app.settings.threads, 1, hw);
    } else {
        ImGui::SameLine();
        ImGui::TextDisabled("(%d)", hw);
    }

    ImGui::SliderInt("Max bounces", &app.settings.max_depth, 1, 10);

    // Resolution scale as a percentage
    int percent = int(app.settings.resolution_scale * 100.0f + 0.5f);
    if (ImGui::SliderInt("Resolution", &percent, 25, 100, "%d%%")) {
        app.settings.resolution_scale = percent / 100.0f;
    }

    // BVH builder (rebuilds the scene)
    int bvh = app.bvh == BvhBuilder::SAH ? 0 : 1;
    const char* bvh_items[] = {"SAH", "Median split"};
    if (ImGui::Combo("BVH", &bvh, bvh_items, 2)) {
        BvhBuilder chosen = bvh == 0 ? BvhBuilder::SAH : BvhBuilder::Median;
        if (chosen != app.bvh) {
            app.bvh = chosen;
            app.request_scene_rebuild = true;
        }
    }

    if (ImGui::Button("Reset camera")) app.request_camera_reset = true;
    ImGui::EndDisabled();
}

void draw_benchmark(AppState& app) {
    if (!ImGui::CollapsingHeader("Benchmark", ImGuiTreeNodeFlags_DefaultOpen)) return;
    ImGui::TextWrapped("Orbits the camera once around the scene using the current settings. "
                       "Times rendering only, like the Benchmark tool.");

    if (app.benchmarking()) {
        char label[64];
        snprintf(label, sizeof(label), "Frame %d / %d", app.bench->frame(), app.bench->frames());
        ImGui::ProgressBar(app.bench->progress(), ImVec2(-1, 0), label);
        if (ImGui::Button("Cancel")) app.request_bench_cancel = true;
    } else {
        ImGui::SetNextItemWidth(120);
        ImGui::InputInt("Frames", &app.bench_frames, 10, 60);
        if (app.bench_frames < 1) app.bench_frames = 1;
        if (ImGui::Button("Run benchmark")) app.request_bench_start = true;
    }

    if (app.has_bench_result) {
        const BenchmarkResult& r = app.last_bench;
        ImGui::Separator();
        ImGui::Text("Last run: %s renderer, %s BVH", app.last_bench_renderer.c_str(), app.last_bench_bvh.c_str());
        ImGui::Text("%d frames at %d x %d, %d threads, %d bounces", r.frames, r.width, r.height, r.threads, r.max_depth);
        if (ImGui::BeginTable("bench", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
            auto row = [](const char* name, const char* fmt, double value) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(name);
                ImGui::TableNextColumn();
                ImGui::Text(fmt, value);
            };
            row("Average FPS", "%.2f", r.fps());
            row("ms/frame", "%.1f", r.ms_per_frame());
            row("Fastest frame", "%.1f ms", 1000.0 * r.min_frame);
            row("Slowest frame", "%.1f ms", 1000.0 * r.max_frame);
            row("Primary rays", "%.2f Mrays/s", r.primary_mrays_per_sec());
            row("All rays", "%.2f Mrays/s", r.all_mrays_per_sec());
            row("Rays per pixel", "%.2f", r.rays_per_pixel());
            ImGui::EndTable();
        }
    }
}

}  // namespace

void draw_ui(AppState& app) {
    if (!app.ui_visible) {
        draw_hint();
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings  (Tab to close)", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        draw_stats(app);
        draw_settings(app);
        draw_benchmark(app);
    }
    ImGui::End();
}
