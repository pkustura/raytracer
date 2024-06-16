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
    float yaw, pitch; // Camera rotation angles

    Camera(Vector3 lookfrom, Vector3 lookat, Vector3 vup, float vfov, float aspect, float aperture);
    
    Ray get_ray(float u, float v) const;
    void update_camera(Vector3 lookfrom, Vector3 lookat, Vector3 vup, float vfov, float aspect, float aperture);
    void update_orientation(float yaw_offset, float pitch_offset);
    void updatePosition(const Vector3& newPosition);
    void updateOrientation(float newYaw, float newPitch);
    void updateVectors();

private:
    float half_width;
    float half_height;
    float aspect_ratio;
    float vertical_fov;

    void update_screen_vectors();
};


