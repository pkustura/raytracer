#include "camera.h"
#include "ray.h"
#include <cmath>

Camera::Camera(Vector3 lookfrom, Vector3 lookat, Vector3 vup, float vfov, float aspect, float aperture) {
    yaw = 0.0f;
    pitch = 0.0f;
    vertical_fov = vfov;
    aspect_ratio = aspect;
    update_camera(lookfrom, lookat, vup, vfov, aspect, aperture);
}

Ray Camera::get_ray(float x, float y) const {
    Vector3 direction = lower_left_corner.clone() + horizontal*x + vertical*y - origin;
    return Ray(origin, direction.normalized());
}

void Camera::update_camera(Vector3 lookfrom, Vector3 lookat, Vector3 vup, float vfov, float aspect, float aperture) {
    vertical_fov = vfov;
    aspect_ratio = aspect;

    float theta = vfov * M_PI / 180.0;
    half_height = tan(theta / 2);
    half_width = aspect * half_height;

    lens_radius = aperture / 2;
    origin = lookfrom;

    updateVectors();
}

void Camera::update_orientation(float yaw_offset, float pitch_offset) {
    yaw += yaw_offset;
    pitch += pitch_offset;

    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;

    float yaw_radians = yaw * M_PI / 180.0f;
    float pitch_radians = pitch * M_PI / 180.0f;

    Vector3 direction;
    direction.x = cos(yaw_radians) * cos(pitch_radians);
    direction.y = sin(pitch_radians);
    direction.z = sin(yaw_radians) * cos(pitch_radians);
    Vector3 lookat = origin + direction;

    update_camera(origin, lookat, Vector3(0, 1, 0), 20.0f, 4.0f / 3.0f, 0.1f);
}

void Camera::updatePosition(const Vector3& newPosition) {
    origin = newPosition;
    update_screen_vectors();
}


void Camera::updateOrientation(float yaw_offset, float pitch_offset) {
    yaw += yaw_offset;
    pitch += pitch_offset;

    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;

    updateVectors();
}

void Camera::updateVectors() {
    Vector3 direction(
        cos(yaw * M_PI / 180.0) * cos(pitch * M_PI / 180.0),
        sin(pitch * M_PI / 180.0),
        sin(yaw * M_PI / 180.0) * cos(pitch * M_PI / 180.0)
    );
    w = (-1 * direction).normalized(); // forward
    u = Vector3(0, 1, 0).cross(w).normalized(); // right
    v = w.cross(u); // up

    update_screen_vectors();
}

void Camera::update_screen_vectors() {
    lower_left_corner = origin - u * half_width - v * half_height - w;
    horizontal = u * (2 * half_width);
    vertical = v * (2 * half_height);
}
