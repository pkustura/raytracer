#include "camera.h"
#include "ray.h"
#include <cmath>

Camera::Camera(Vector3 lookfrom, Vector3 lookat, Vector3 vup, float vfov, float aspect, float aperture) {
   update_camera(lookfrom, lookat, vup, vfov, aspect, aperture);
}

Ray Camera::get_ray(float x, float y) const {
    Vector3 direction = lower_left_corner.clone() + horizontal*x + vertical*y - origin;
    return Ray(origin, direction.normalized());
}

void Camera::update_camera(Vector3 lookfrom, Vector3 lookat, Vector3 vup, float vfov, float aspect, float aperture) {
    Vector3 u, v, w;
    float theta = vfov * M_PI / 180.0;
    float half_height = tan(theta / 2);
    float half_width = aspect * half_height;

    lens_radius = aperture / 2;
    origin = lookfrom;

    //camera basis vectors
    w = (lookfrom - lookat).normalized();
    u = vup.cross(w).normalized();
    v = w.cross(u);

    lower_left_corner = origin - u*half_width - v*half_height - w;
    horizontal = u* (2*half_width);
    vertical = v*(2*half_height);

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


