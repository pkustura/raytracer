#pragma once
#include <array>
#include <string>
#include <vector>
#include "vector.h"

// Minimal Wavefront OBJ loader (ported from ascii-render's load_obj).
// Only vertex ("v") and face ("f") lines are read; texture coordinates,
// normals, groups and materials are ignored. Polygons with more than three
// vertices are fan-triangulated.
//
// When `normalize` is true the model is centered on its vertex average and
// scaled so its largest per-axis extent from the center is 1 (fits in [-1, 1]).
//
// Returns false if the file can't be opened or contains no usable geometry.
bool load_obj(const std::string& filename,
              std::vector<Vector3>& vertices,
              std::vector<std::array<int, 3>>& faces,
              bool normalize = true);
