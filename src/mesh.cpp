#include "mesh.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include "obj_loader.h"

namespace {

const float TRI_EPSILON = 1e-8f;
const int LEAF_SIZE = 4;  // used only to size the node reservation

// Möller–Trumbore ray/triangle test. v0 is a vertex, e1/e2 the edges from it.
bool intersect_triangle(const Ray& ray, const Vector3& v0, const Vector3& e1, const Vector3& e2, float& t) {
    Vector3 p = ray.direction.cross(e2);
    float det = e1.dot(p);
    if (std::fabs(det) < TRI_EPSILON) return false;  // ray parallel to triangle

    float inv_det = 1.0f / det;
    Vector3 s = ray.origin - v0;
    float u = s.dot(p) * inv_det;
    if (u < 0.0f || u > 1.0f) return false;

    Vector3 q = s.cross(e1);
    float v = ray.direction.dot(q) * inv_det;
    if (v < 0.0f || u + v > 1.0f) return false;

    t = e2.dot(q) * inv_det;
    return t > 1e-4f;  // ignore self-hits right at the ray origin
}

float axis(const Vector3& v, int a) {
    return a == 0 ? v.x : (a == 1 ? v.y : v.z);
}

Vector3 vmin(const Vector3& a, const Vector3& b) {
    return Vector3(std::fmin(a.x, b.x), std::fmin(a.y, b.y), std::fmin(a.z, b.z));
}

Vector3 vmax(const Vector3& a, const Vector3& b) {
    return Vector3(std::fmax(a.x, b.x), std::fmax(a.y, b.y), std::fmax(a.z, b.z));
}

// Slab test; returns true if the ray hits the box closer than t_max.
bool hit_box(const Vector3& bmin, const Vector3& bmax, const Vector3& origin, const Vector3& inv_dir, float t_max) {
    float t0 = 0.0f, t1 = t_max;
    for (int a = 0; a < 3; ++a) {
        float inv = axis(inv_dir, a);
        float tn = (axis(bmin, a) - axis(origin, a)) * inv;
        float tf = (axis(bmax, a) - axis(origin, a)) * inv;
        if (tn > tf) std::swap(tn, tf);
        t0 = tn > t0 ? tn : t0;
        t1 = tf < t1 ? tf : t1;
        if (t0 > t1) return false;
    }
    return true;
}

}  // namespace

// ---------------------------------------------------------------- Triangle

Triangle::Triangle(const Vector3& v0, const Vector3& v1, const Vector3& v2, const Material& material)
    : Shape(material), v0(v0), v1(v1), v2(v2), normal((v1 - v0).cross(v2 - v0).normalized()) {}

bool Triangle::intersect(const Ray& ray, float& t) const {
    return intersect_triangle(ray, v0, v1 - v0, v2 - v0, t);
}

Vector3 Triangle::get_normal(const Vector3& point) const {
    return normal;
}

// -------------------------------------------------------------------- Mesh

Mesh::Mesh(const std::vector<Vector3>& vertices,
           const std::vector<std::array<int, 3>>& faces,
           const Material& material,
           const Vector3& position,
           float scale)
    : Shape(material) {
    tris.reserve(faces.size());
    std::vector<Vector3> centroids;
    centroids.reserve(faces.size());

    for (const auto& f : faces) {
        Vector3 a = vertices[f[0]] * scale + position;
        Vector3 b = vertices[f[1]] * scale + position;
        Vector3 c = vertices[f[2]] * scale + position;
        Vector3 e1 = b - a, e2 = c - a;
        Vector3 n = e1.cross(e2);
        if (n.length() == 0.0f) continue;  // skip degenerate triangles
        tris.push_back({a, e1, e2, n.normalized()});
        centroids.push_back((a + b + c) / 3.0f);
    }

    if (!tris.empty()) {
        nodes.reserve(2 * tris.size() / LEAF_SIZE + 1);
        build(0, static_cast<int>(tris.size()), centroids, 0);
    }
}

Mesh Mesh::from_obj(const std::string& filename, const Material& material, const Vector3& position, float scale) {
    std::vector<Vector3> vertices;
    std::vector<std::array<int, 3>> faces;
    load_obj(filename, vertices, faces, true);  // on failure both are empty -> empty mesh
    return Mesh(vertices, faces, material, position, scale);
}

namespace {

const int SAH_BINS = 12;
const int MAX_LEAF_SIZE = 8;     // nodes above this size are always split
const int MEDIAN_SPLIT_DEPTH = 48;  // past this depth, use median splits to bound tree depth

struct Bin {
    Vector3 bmin{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    Vector3 bmax{-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max()};
    int count = 0;

    void grow(const Vector3& lo, const Vector3& hi) {
        bmin = vmin(bmin, lo);
        bmax = vmax(bmax, hi);
    }
    void grow(const Bin& other) {
        bmin = vmin(bmin, other.bmin);
        bmax = vmax(bmax, other.bmax);
        count += other.count;
    }
    float area() const {
        if (count == 0) return 0.0f;
        Vector3 d = bmax - bmin;
        return 2.0f * (d.x * d.y + d.y * d.z + d.z * d.x);
    }
};

int bin_index(float c, float lo, float extent) {
    int b = static_cast<int>((c - lo) / extent * SAH_BINS);
    return b < 0 ? 0 : (b >= SAH_BINS ? SAH_BINS - 1 : b);
}

}  // namespace

// Recursively builds the BVH over tris[start, end) using the surface area
// heuristic (binned). Returns the new node's index.
int Mesh::build(int start, int end, std::vector<Vector3>& centroids, int depth) {
    int index = static_cast<int>(nodes.size());
    nodes.emplace_back();

    // Bounds of the triangles and of their centroids
    Vector3 bmin = tris[start].v0, bmax = tris[start].v0;
    Vector3 cmin = centroids[start], cmax = centroids[start];
    for (int i = start; i < end; ++i) {
        const Tri& tri = tris[i];
        Vector3 b = tri.v0 + tri.e1, c = tri.v0 + tri.e2;
        bmin = vmin(bmin, vmin(tri.v0, vmin(b, c)));
        bmax = vmax(bmax, vmax(tri.v0, vmax(b, c)));
        cmin = vmin(cmin, centroids[i]);
        cmax = vmax(cmax, centroids[i]);
    }
    nodes[index].box = {bmin, bmax};

    int count = end - start;
    auto make_leaf = [&]() {
        nodes[index].start = start;
        nodes[index].count = count;
        return index;
    };
    if (count <= 2) return make_leaf();

    Vector3 extent = cmax - cmin;
    int mid = -1;

    if (depth < MEDIAN_SPLIT_DEPTH) {
        // Evaluate SAH cost for every bin boundary on every axis
        float best_cost = std::numeric_limits<float>::max();
        int best_axis = -1, best_split = 0;

        for (int a = 0; a < 3; ++a) {
            float lo = axis(cmin, a), ext = axis(extent, a);
            if (ext <= 0.0f) continue;

            Bin bins[SAH_BINS];
            for (int i = start; i < end; ++i) {
                const Tri& tri = tris[i];
                Vector3 b = tri.v0 + tri.e1, c = tri.v0 + tri.e2;
                Bin& bin = bins[bin_index(axis(centroids[i], a), lo, ext)];
                bin.grow(vmin(tri.v0, vmin(b, c)), vmax(tri.v0, vmax(b, c)));
                bin.count++;
            }

            // Sweep from the right to get areas/counts of every right-hand side
            float right_area[SAH_BINS];
            int right_count[SAH_BINS];
            Bin acc;
            for (int k = SAH_BINS - 1; k > 0; --k) {
                acc.grow(bins[k]);
                right_area[k] = acc.area();
                right_count[k] = acc.count;
            }

            // Sweep from the left; split k puts bins [0, k) left and [k, N) right
            acc = Bin();
            for (int k = 1; k < SAH_BINS; ++k) {
                acc.grow(bins[k - 1]);
                if (acc.count == 0 || right_count[k] == 0) continue;
                float cost = acc.area() * acc.count + right_area[k] * right_count[k];
                if (cost < best_cost) {
                    best_cost = cost;
                    best_axis = a;
                    best_split = k;
                }
            }
        }

        if (best_axis < 0) return make_leaf();  // all centroids coincide

        // Compare against the cost of intersecting every triangle in a leaf
        Vector3 d = bmax - bmin;
        float parent_area = 2.0f * (d.x * d.y + d.y * d.z + d.z * d.x);
        float split_cost = 1.0f + best_cost / parent_area;
        if (split_cost >= count && count <= MAX_LEAF_SIZE) return make_leaf();

        // Partition triangles (and their centroids) by bin
        float lo = axis(cmin, best_axis), ext = axis(extent, best_axis);
        int i = start, j = end - 1;
        while (i <= j) {
            if (bin_index(axis(centroids[i], best_axis), lo, ext) < best_split) {
                ++i;
            } else {
                std::swap(tris[i], tris[j]);
                std::swap(centroids[i], centroids[j]);
                --j;
            }
        }
        mid = i;
    }

    if (mid <= start || mid >= end) {
        // Too deep (or partition failed): median split along the widest centroid axis
        int split_axis = 0;
        if (extent.y > extent.x) split_axis = 1;
        if (extent.z > axis(extent, split_axis)) split_axis = 2;
        if (axis(extent, split_axis) <= 0.0f) return make_leaf();

        mid = start + count / 2;
        std::vector<int> order(count);
        std::iota(order.begin(), order.end(), start);
        std::nth_element(order.begin(), order.begin() + (mid - start), order.end(), [&](int x, int y) {
            return axis(centroids[x], split_axis) < axis(centroids[y], split_axis);
        });
        std::vector<Tri> tmp_tris(count);
        std::vector<Vector3> tmp_centroids(count);
        for (int k = 0; k < count; ++k) {
            tmp_tris[k] = tris[order[k]];
            tmp_centroids[k] = centroids[order[k]];
        }
        std::copy(tmp_tris.begin(), tmp_tris.end(), tris.begin() + start);
        std::copy(tmp_centroids.begin(), tmp_centroids.end(), centroids.begin() + start);
    }

    int left = build(start, mid, centroids, depth + 1);
    int right = build(mid, end, centroids, depth + 1);
    nodes[index].left = left;
    nodes[index].right = right;
    return index;
}

bool Mesh::intersect(const Ray& ray, float& t, Vector3& normal) const {
    if (nodes.empty()) return false;

    Vector3 inv_dir(1.0f / ray.direction.x, 1.0f / ray.direction.y, 1.0f / ray.direction.z);
    float closest = std::numeric_limits<float>::max();
    int hit = -1;

    int stack[128];  // build() bounds tree depth to well under this
    int sp = 0;
    stack[sp++] = 0;
    while (sp > 0) {
        const Node& node = nodes[stack[--sp]];
        if (!hit_box(node.box.min, node.box.max, ray.origin, inv_dir, closest)) continue;

        if (node.left < 0) {
            for (int i = node.start; i < node.start + node.count; ++i) {
                float t_tri;
                if (intersect_triangle(ray, tris[i].v0, tris[i].e1, tris[i].e2, t_tri) && t_tri < closest) {
                    closest = t_tri;
                    hit = i;
                }
            }
        } else {
            stack[sp++] = node.left;
            stack[sp++] = node.right;
        }
    }

    if (hit < 0) return false;
    t = closest;
    normal = tris[hit].normal;
    return true;
}

bool Mesh::intersect(const Ray& ray, float& t) const {
    // Not thread-safe (writes last_normal); the renderer uses the overload above.
    return intersect(ray, t, last_normal);
}

Vector3 Mesh::get_normal(const Vector3& point) const {
    return last_normal;
}
