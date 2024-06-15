#pragma once
#include "vector.h"

class Color {
public:
    float r, g, b;

    Color() : r(0), g(0), b(0) {}
    Color(float r, float g, float b) : r(r), g(g), b(b) {}

    Color operator+(const Color& c) const;
    Color operator*(float scalar) const;
    Color operator*(const Vector3& v) const;  // Intensity is vec3.
    Color& operator+=(const Color& c);
    Color& operator*=(float scalar);

    // Clamp color values to [0, 1]
    Color clamped() const;
};

