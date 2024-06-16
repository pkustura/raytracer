#include "vector.h"
#include <cmath>

// Vector addition
Vector3 Vector3::operator+(const Vector3& v) const {
    return Vector3(x + v.x, y + v.y, z + v.z);
}

// Vector subtraction
Vector3 Vector3::operator-(const Vector3& v) const {
    return Vector3(x - v.x, y - v.y, z - v.z);
}

// Scalar multiplication
Vector3 Vector3::operator*(float scalar) const {
    return Vector3(x * scalar, y * scalar, z * scalar);
}

// Scalar division
Vector3 Vector3::operator/(float scalar) const {
    return Vector3(x / scalar, y / scalar, z / scalar);
}

// In-place addition
Vector3& Vector3::operator+=(const Vector3& v) {
    x += v.x;
    y += v.y;
    z += v.z;
    return *this;
}

// In-place subtraction
Vector3& Vector3::operator-=(const Vector3& v) {
    x -= v.x;
    y -= v.y;
    z -= v.z;
    return *this;
}

// In-place scalar multiplication
Vector3& Vector3::operator*=(float scalar) {
    x *= scalar;
    y *= scalar;
    z *= scalar;
    return *this;
}

// In-place scalar division
Vector3& Vector3::operator/=(float scalar) {
    x /= scalar;
    y /= scalar;
    z /= scalar;
    return *this;
}

// Dot product
float Vector3::dot(const Vector3& v) const {
    return x * v.x + y * v.y + z * v.z;
}

// Cross product
Vector3 Vector3::cross(const Vector3& v) const {
    return Vector3(
        y * v.z - z * v.y,
        z * v.x - x * v.z,
        x * v.y - y * v.x
    );
}

// Length (magnitude) of the vector
float Vector3::length() const {
    return std::sqrt(x * x + y * y + z * z);
}

// Normalized (unit) vector
Vector3 Vector3::normalized() const {
    float len = length();
    return Vector3(x / len, y / len, z / len);
}

// Clone
Vector3 Vector3::clone() const {
    return Vector3(x, y, z);
}

Vector3 Vector3::addScaledVector(Vector3 v, float scalar) const {
  Vector3 sv = v*scalar;
  return Vector3(x + sv.x, y + sv.y, z + sv.z);
}

// Friend function to support float * Vector3
Vector3 operator*(float scalar, const Vector3& v) {
    return Vector3(v.x * scalar, v.y * scalar, v.z * scalar);
}

