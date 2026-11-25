#include "scene.h"
#include <algorithm>
#include <iostream>
#include "material.h"
#include "mesh.h"
#include "obj_loader.h"

#ifndef RAYTRACER_ASSET_DIR
#define RAYTRACER_ASSET_DIR "objs"
#endif

std::string asset_path(const std::string& name) {
    return std::string(RAYTRACER_ASSET_DIR) + "/" + name;
}

Scene::Scene()
    : light(Vector3(5, 5, 5), Vector3(1, 1, 1)),
      camera_from(0, 1.5f, 10),
      camera_at(0, -0.3f, 0),
      vfov(20.0f) {}

Scene::~Scene() {
    for (auto shape : shapes) delete shape;
}

namespace {

// Loads a model and places it so its lowest point rests on the plane y = floor_y.
// `scale` is applied to the model after it has been normalized to fit [-1, 1].
bool add_model(Scene& scene, const std::string& file, const Material& material,
               float x, float z, float floor_y, float scale, BvhBuilder builder) {
    std::vector<Vector3> vertices;
    std::vector<std::array<int, 3>> faces;
    if (!load_obj(asset_path(file), vertices, faces)) return false;

    float min_y = vertices[0].y;
    for (const auto& v : vertices) min_y = std::min(min_y, v.y);

    Vector3 position(x, floor_y - min_y * scale, z);
    scene.shapes.push_back(new Mesh(vertices, faces, material, position, scale, builder));
    return true;
}

}  // namespace

bool build_default_scene(Scene& scene, BvhBuilder builder) {
    const float floor_y = -1.0f;

    DiffuseMaterial floorMat(Color(0.1f, 0.1f, 0.1f), Color(0.8f, 0.4f, 0.3f));
    PhongMaterial dragonMat(Color(0.1f, 0.1f, 0.1f), Color(0.1f, 0.5f, 0.3f), Color(1.0f, 1.0f, 1.0f), 10.0f);
    PhongMaterial buddhaMat(Color(0.1f, 0.08f, 0.02f), Color(0.8f, 0.6f, 0.2f), Color(1.0f, 0.9f, 0.6f), 30.0f);
    MirrorMaterial mirrorMat(Color(0.1f, 0.1f, 0.1f), Color(0.9f, 0.9f, 0.9f));
    GlassMaterial glassMat(Color(0.1f, 0.1f, 0.1f), Color(0.1f, 0.1f, 0.1f), 0.9f, 1.5f);

    scene.shapes.push_back(new Plane(Vector3(0, floor_y, 0), Vector3(0, 1, 0), floorMat));

    bool ok = true;
    ok &= add_model(scene, "buddha.obj", buddhaMat, -1.9f, -0.3f, floor_y, 1.0f, builder);
    ok &= add_model(scene, "dragon.obj", dragonMat, 0.2f, 0.0f, floor_y, 1.3f, builder);
    ok &= add_model(scene, "teapot.obj", mirrorMat, 1.8f, -0.4f, floor_y, 0.5f, builder);

    // Glass sphere in front of the dragon
    scene.shapes.push_back(new Sphere(Vector3(0.9f, floor_y + 0.45f, 1.6f), 0.45f, glassMat));

    if (!ok) {
        std::cerr << "build_default_scene: failed to load one or more models from "
                  << RAYTRACER_ASSET_DIR << " (run ./fetch_models.sh to download them)" << std::endl;
    }
    return ok;
}
