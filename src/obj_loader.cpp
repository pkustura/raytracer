#include "obj_loader.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>

bool load_obj(const std::string& filename,
              std::vector<Vector3>& vertices,
              std::vector<std::array<int, 3>>& faces,
              bool normalize) {
    std::ifstream file(filename);
    if (!file) {
        std::cerr << "load_obj: could not open " << filename << std::endl;
        return false;
    }

    vertices.clear();
    faces.clear();

    std::string line;
    std::vector<int> polygon;
    while (std::getline(file, line)) {
        if (line.size() < 2) continue;

        if (line[0] == 'v' && line[1] == ' ') {
            std::istringstream ss(line.substr(2));
            float x, y, z;
            if (ss >> x >> y >> z) {
                vertices.emplace_back(x, y, z);
            }
        } else if (line[0] == 'f' && line[1] == ' ') {
            std::istringstream ss(line.substr(2));
            std::string token;
            polygon.clear();
            while (ss >> token) {
                // Tokens look like "v", "v/vt", "v//vn" or "v/vt/vn"; keep only v.
                int index = std::stoi(token.substr(0, token.find('/')));
                // OBJ indices are 1-based; negative indices count back from the end.
                index = index > 0 ? index - 1 : static_cast<int>(vertices.size()) + index;
                polygon.push_back(index);
            }
            // Fan-triangulate (handles triangles, quads and larger convex polygons)
            for (size_t i = 1; i + 1 < polygon.size(); ++i) {
                faces.push_back({polygon[0], polygon[i], polygon[i + 1]});
            }
        }
    }

    // Drop faces that reference vertices that don't exist
    size_t valid = 0;
    for (const auto& f : faces) {
        bool ok = true;
        for (int idx : f) ok = ok && idx >= 0 && idx < static_cast<int>(vertices.size());
        if (ok) faces[valid++] = f;
    }
    if (valid != faces.size()) {
        std::cerr << "load_obj: skipped " << faces.size() - valid
                  << " faces with invalid indices in " << filename << std::endl;
        faces.resize(valid);
    }

    if (vertices.empty() || faces.empty()) {
        std::cerr << "load_obj: no geometry in " << filename << std::endl;
        return false;
    }

    if (normalize) {
        // Center on the vertex average
        Vector3 center;
        for (const auto& v : vertices) center += v;
        center /= static_cast<float>(vertices.size());

        // Scale by the largest per-axis distance from the center
        float max_distance = 0.0f;
        for (const auto& v : vertices) {
            max_distance = std::fmax(max_distance, std::fabs(v.x - center.x));
            max_distance = std::fmax(max_distance, std::fabs(v.y - center.y));
            max_distance = std::fmax(max_distance, std::fabs(v.z - center.z));
        }
        float scale = max_distance > 0.0f ? 1.0f / max_distance : 1.0f;

        for (auto& v : vertices) v = (v - center) * scale;
    }

    return true;
}
