#pragma once
#include "vector.h"

class Camera {
public:
    Vector3 origin;
    Vector3 lower_left_corner;
    Vector3 horizontal;
    Vector3 vertical;

    Camera(Vector3 lookfrom, Vector3 lookat, Vector3 vup, float vfov, float aspect);
    
    Ray get_ray(float u, float v) const;
};

