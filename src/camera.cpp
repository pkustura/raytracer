#include "camera.h"
#include "ray.h"
#include <cmath>

Camera::Camera(Vector3 lookfrom, Vector3 lookat, Vector3 vup, float vfov, float aspect) {
    Vector3 u, v, w;
    float theta = vfov * M_PI / 180.0;
    float half_height = tan(theta / 2);
    float half_width = aspect * half_height;

    origin = lookfrom;
    w = (lookfrom - lookat).normalized();
    u = vup.cross(w).normalized();
    v = w.cross(u);

    lower_left_corner = origin - half_width*u - half_height*v - w;
    horizontal = 2 * half_width * u;
    vertical = 2 * half_height * v;
}

Ray Camera::get_ray(float u, float v) const {
    return Ray(origin, lower_left_corner + u*horizontal + v*vertical - origin);
}

