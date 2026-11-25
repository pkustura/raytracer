#pragma once
#include <string>
#include <vector>
#include "camera.h"
#include "light.h"
#include "shape.h"

// Resolves a model file name (e.g. "dragon.obj") to a path inside the repo's
// objs/ directory. The directory is baked in at compile time, so the program
// works no matter which directory it is run from.
std::string asset_path(const std::string& name);

struct Scene {
    std::vector<Shape*> shapes;
    Light light;

    // Where the camera starts and what it looks at.
    Vector3 camera_from;
    Vector3 camera_at;
    float vfov;

    Scene();
    ~Scene();
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;
};

// Dragon, Buddha and a mirrored teapot on a floor, with a glass sphere in front.
// Returns false if a model failed to load.
bool build_default_scene(Scene& scene);
