#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include <GL/glew.h>  // Include GLEW before GLFW
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl2.h"
#include "app_ui.h"
#include "benchmark.h"
#include "camera.h"
#include "mesh.h"
#include "pathtracer.h"
#include "renderer.h"
#include "scene.h"

using namespace std;
using Clock = chrono::steady_clock;

namespace {

const float MOVE_SPEED = 4.0f;        // world units per second
const float FAST_MULTIPLIER = 3.0f;   // while Shift is held
const float MAX_STEP_SECONDS = 0.1f;  // cap per-frame movement after a hitch

// Moves the camera with WASD. Movement is scaled by the frame time `dt`
// (seconds), so speed doesn't depend on the frame rate.
void processInput(GLFWwindow* window, Camera* camera, float dt) {
    auto down = [&](int key) { return glfwGetKey(window, key) == GLFW_PRESS; };

    Vector3 move(0, 0, 0);
    if (down(GLFW_KEY_W)) move -= camera->w;  // forward (w points backward)
    if (down(GLFW_KEY_S)) move += camera->w;
    if (down(GLFW_KEY_A)) move -= camera->u;
    if (down(GLFW_KEY_D)) move += camera->u;
    if (move.length() == 0.0f) return;

    float speed = MOVE_SPEED;
    if (down(GLFW_KEY_LEFT_SHIFT) || down(GLFW_KEY_RIGHT_SHIFT)) speed *= FAST_MULTIPLIER;

    // Normalize so diagonal movement isn't faster than straight movement
    float step = speed * std::min(dt, MAX_STEP_SECONDS);
    camera->updatePosition(camera->origin + move.normalized() * step);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    static const float sensitivity = 0.1f;  // Mouse sensitivity
    static float lastX = 400, lastY = 300;

    AppState* app = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (!app) return;

    // Mouse look only while the cursor is captured (settings closed, no benchmark)
    if (app->ui_visible || app->benchmarking()) {
        app->reset_mouse = true;
        return;
    }
    if (app->reset_mouse) {
        lastX = xpos;
        lastY = ypos;
        app->reset_mouse = false;
        return;
    }

    float xoffset = (xpos - lastX) * sensitivity;
    float yoffset = (lastY - ypos) * sensitivity;  // Reversed since y-coordinates go from bottom to top
    lastX = xpos;
    lastY = ypos;

    app->camera->updateOrientation(xoffset, yoffset);
}

Camera make_camera(const Scene& scene, int width, int height) {
    return Camera(scene.camera_from, scene.camera_at, Vector3(0, 1, 0), scene.vfov,
                  float(width) / float(height), 0.1f);
}

void load_scene(AppState& app) {
    auto t0 = Clock::now();
    auto scene = make_unique<Scene>();
    app.scene_ok = build_default_scene(*scene, app.bvh);
    app.scene_load_seconds = chrono::duration<double>(Clock::now() - t0).count();

    app.triangle_count = 0;
    for (Shape* shape : scene->shapes) {
        if (auto* mesh = dynamic_cast<Mesh*>(shape)) app.triangle_count += mesh->triangle_count();
    }
    app.scene = std::move(scene);
}

void set_cursor_captured(GLFWwindow* window, AppState& app, bool captured) {
    glfwSetInputMode(window, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    app.reset_mouse = true;
}

// Uploads the rendered image to `texture`, resizing the texture if needed.
void upload_image(GLuint texture, const vector<Color>& image, int width, int height,
                  int& tex_width, int& tex_height, vector<unsigned char>& pixels) {
    pixels.resize(size_t(width) * height * 3);
    for (size_t i = 0; i < image.size(); ++i) {
        pixels[i * 3 + 0] = static_cast<unsigned char>(255.99f * image[i].r);
        pixels[i * 3 + 1] = static_cast<unsigned char>(255.99f * image[i].g);
        pixels[i * 3 + 2] = static_cast<unsigned char>(255.99f * image[i].b);
    }

    glBindTexture(GL_TEXTURE_2D, texture);
    if (width != tex_width || height != tex_height) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
        tex_width = width;
        tex_height = height;
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    }
}

void draw_fullscreen_texture(GLuint texture) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f, -1.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex2f( 1.0f, -1.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex2f( 1.0f,  1.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f,  1.0f);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

}  // namespace

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        cerr << "Failed to initialize GLFW" << endl;
        return -1;
    }

    AppState app;

    // Fixed-size window: the render resolution and camera aspect follow it
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(app.window_width, app.window_height, "Raytracer", NULL, NULL);
    if (!window) {
        cerr << "Failed to create GLFW window" << endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);  // don't cap the frame rate; we want to see real render speed

    // Initialize GLEW
    if (glewInit() != GLEW_OK) {
        cerr << "Failed to initialize GLEW" << endl;
        return -1;
    }

    // Scene, camera and renderers
    load_scene(app);
    app.camera = make_unique<Camera>(make_camera(*app.scene, app.window_width, app.window_height));
    app.renderers.push_back(make_unique<CpuRenderer>());
    app.renderers.push_back(make_unique<PathTracer>());

    // Input: our mouse callback first, so ImGui's backend chains to it
    glfwSetWindowUserPointer(window, &app);
    glfwSetCursorPosCallback(window, mouse_callback);
    set_cursor_captured(window, app, true);

    // Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;  // don't write imgui.ini next to the binary
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL2_Init();

    // Display texture
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);  // rows of RGB bytes aren't always 4-byte aligned
    int tex_width = 0, tex_height = 0;

    vector<Color> image;
    vector<unsigned char> pixels;
    bool tab_was_down = false;
    auto last_frame = Clock::now();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGuiIO& io = ImGui::GetIO();

        // Time since the previous frame, used for movement and the FPS counter
        auto now = Clock::now();
        double frame_s = chrono::duration<double>(now - last_frame).count();
        last_frame = now;

        // Keyboard: Esc quits, Tab toggles the settings panel
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
        bool tab_down = glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;
        if (tab_down && !tab_was_down && !io.WantTextInput) {
            app.ui_visible = !app.ui_visible;
            set_cursor_captured(window, app, !app.ui_visible);
        }
        tab_was_down = tab_down;

        // Handle requests from the UI
        if (app.request_scene_rebuild) {
            app.request_scene_rebuild = false;
            load_scene(app);
            for (auto& r : app.renderers) r->reset();
        }
        if (app.request_camera_reset) {
            app.request_camera_reset = false;
            *app.camera = make_camera(*app.scene, app.window_width, app.window_height);
        }

        int width = max(1, int(app.window_width * app.settings.resolution_scale));
        int height = max(1, int(app.window_height * app.settings.resolution_scale));

        if (app.request_bench_start) {
            app.request_bench_start = false;
            app.bench = make_unique<BenchmarkRun>(*app.scene, app.bench_frames, width, height,
                                                  resolve_threads(app.settings.threads), app.settings.max_depth);
            if (app.renderer().progressive()) app.bench->set_samples_per_frame(app.settings.samples_per_frame);
            app.renderer().reset();  // start from a clean state, like the headless tool
        }
        if (app.request_bench_cancel) {
            app.request_bench_cancel = false;
            app.bench.reset();
        }

        // Pick the camera: the benchmark orbit, or the user's camera
        Camera camera = app.benchmarking() ? app.bench->camera() : *app.camera;
        if (app.benchmarking()) {
            width = app.bench->result().width;
            height = app.bench->result().height;
        } else if (!io.WantCaptureKeyboard) {
            processInput(window, app.camera.get(), float(frame_s));
            camera = *app.camera;
        }

        // Render
        auto t0 = Clock::now();
        RenderSettings settings = app.settings;
        if (app.benchmarking()) settings.max_samples = 0;  // every frame renders samples_per_frame, as headless does
        uint64_t rays = app.renderer().render(camera, *app.scene, settings, width, height, image);
        double render_s = chrono::duration<double>(Clock::now() - t0).count();

        if (app.benchmarking()) {
            app.bench->record(render_s, rays);
            if (app.bench->done()) {
                app.last_bench = app.bench->result();
                app.last_bench_renderer = app.renderer().name();
                app.last_bench_bvh = bvh_name(app.bvh);
                app.has_bench_result = true;
                app.bench.reset();
            }
        }

        // Smoothed stats
        const double k = 0.1;
        app.render_ms += k * (1000.0 * render_s - app.render_ms);
        app.frame_ms += k * (1000.0 * frame_s - app.frame_ms);
        app.mrays_per_sec += k * ((render_s > 0 ? rays / render_s / 1e6 : 0.0) - app.mrays_per_sec);
        app.render_width = width;
        app.render_height = height;

        // A progressive renderer that hit its sample cap adds nothing new: skip
        // the upload and idle a little instead of spinning a core at 100%
        bool converged = rays == 0 && app.renderer().progressive() && !app.benchmarking();
        if (converged && width == tex_width && height == tex_height) {
            this_thread::sleep_for(chrono::milliseconds(15));
        } else {
            upload_image(texture, image, width, height, tex_width, tex_height, pixels);
        }

        // Display the image, then the UI on top

        int fb_width, fb_height;
        glfwGetFramebufferSize(window, &fb_width, &fb_height);
        glViewport(0, 0, fb_width, fb_height);
        glClear(GL_COLOR_BUFFER_BIT);
        draw_fullscreen_texture(texture);

        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        draw_ui(app);
        ImGui::Render();
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Clean up
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glDeleteTextures(1, &texture);
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
