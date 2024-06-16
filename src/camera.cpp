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


