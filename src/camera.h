#pragma once
#include "vector.h"
#include "ray.h"

class Camera {
public:
    Vector3 origin;
    Vector3 lower_left_corner;
    Vector3 horizontal;
    Vector3 vertical;
    Vector3 u, v, w;  // Camera basis vectors
    float lens_radius;

    Camera(Vector3 lookfrom, Vector3 lookat, Vector3 vup, float vfov, float aspect, float aperture);
    
    Ray get_ray(float u, float v) const;
    void update_camera(Vector3 lookfrom, Vector3 lookat, Vector3 vup, float vfov, float aspect, float aperture);
};

