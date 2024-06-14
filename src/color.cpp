#include "color.h"

// Color addition
Color Color::operator+(const Color& c) const {
    return Color(r + c.r, g + c.g, b + c.b);
}

// Scalar multiplication
Color Color::operator*(float scalar) const {
    return Color(r * scalar, g * scalar, b * scalar);
}

// In-place addition
Color& Color::operator+=(const Color& c) {
    r += c.r;
    g += c.g;
    b += c.b;
    return *this;
}

// In-place scalar multiplication
Color& Color::operator*=(float scalar) {
    r *= scalar;
    g *= scalar;
    b *= scalar;
    return *this;
}

// Clamp color values to [0, 1]
Color Color::clamped() const {
    return Color(
        r < 0 ? 0 : (r > 1 ? 1 : r),
        g < 0 ? 0 : (g > 1 ? 1 : g),
        b < 0 ? 0 : (b > 1 ? 1 : b)
    );
}

