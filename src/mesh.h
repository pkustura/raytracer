#pragma once
#include <array>
#include <string>
#include <vector>
#include "shape.h"

// A single triangle (Möller–Trumbore intersection, flat face normal).
class Triangle : public Shape {
public:
    Vector3 v0, v1, v2;
    Vector3 normal;

    Triangle(const Vector3& v0, const Vector3& v1, const Vector3& v2, const Material& material);

    bool intersect(const Ray& ray, float& t) const override;
    Vector3 get_normal(const Vector3& point) const override;
};

// How a Mesh builds its BVH.
enum class BvhBuilder {
    Median,  // split at the median centroid of the widest axis (leaves of <= 4 triangles)
    SAH,     // binned surface area heuristic (default)
};

// Triangle mesh with a bounding volume hierarchy, so models with hundreds of
// thousands of triangles (dragon, buddha) can be traced.
class Mesh : public Shape {
public:
    // Builds a mesh from vertices/faces, placing it at `position` and scaling by `scale`.
    Mesh(const std::vector<Vector3>& vertices,
         const std::vector<std::array<int, 3>>& faces,
         const Material& material,
         const Vector3& position = Vector3(0, 0, 0),
         float scale = 1.0f,
         BvhBuilder builder = BvhBuilder::SAH);

    // Loads an OBJ file (normalized to fit [-1, 1]) and builds the mesh.
    // Check empty() afterwards to see whether loading worked.
    static Mesh from_obj(const std::string& filename,
                         const Material& material,
                         const Vector3& position = Vector3(0, 0, 0),
                         float scale = 1.0f,
                         BvhBuilder builder = BvhBuilder::SAH);

    bool intersect(const Ray& ray, float& t) const override;
    bool intersect(const Ray& ray, float& t, Vector3& normal) const override;

    // Returns the normal of the most recent hit from intersect(ray, t).
    // Prefer the three-argument intersect, which returns the normal directly
    // and is safe to call from multiple threads.
    Vector3 get_normal(const Vector3& point) const override;

    bool empty() const { return tris.empty(); }
    size_t triangle_count() const { return tris.size(); }

private:
    struct Tri {
        Vector3 v0, e1, e2;  // first vertex and two edges
        Vector3 normal;
    };

    struct AABB {
        Vector3 min, max;
    };

    struct Node {
        AABB box;
        int left = -1;   // child index (interior), -1 for leaf
        int right = -1;
        int start = 0;   // first triangle (leaf)
        int count = 0;   // triangle count (leaf)
    };

    std::vector<Tri> tris;
    std::vector<Node> nodes;
    mutable Vector3 last_normal;
    BvhBuilder builder;

    int build(int start, int end, std::vector<Vector3>& centroids, int depth);
};
